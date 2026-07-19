/*
 * Threshold + movement-gap + activation-delay temporary layer processor
 * for ZMK v0.3.x.
 *
 * param1: target layer
 * param2: inactivity timeout while the layer is active, in milliseconds
 *
 * Design goals:
 * - Pointer events are never modified or consumed.
 * - While active, the hot path performs only an atomic timestamp write.
 * - A separate delayable work item checks inactivity at low frequency.
 * - 32-bit uptime arithmetic is wrap-safe and atomic on nRF52840.
 */
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer
#define TIMEOUT_CHECK_INTERVAL_MS 100U

struct threshold_layer_config {
    uint32_t threshold;
    uint32_t movement_gap_ms;
    uint32_t activation_delay_ms;
};

struct threshold_layer_data {
    uint32_t accumulated;
    uint32_t timeout_ms;
    uint32_t sequence_started_ms;

    /* Atomic because input callbacks and the system workqueue share these. */
    atomic_t active_layer;
    atomic_t last_motion_ms;

    struct k_work_delayable timeout_check_work;
};

static void reset_pending_sequence(struct threshold_layer_data *data) {
    data->accumulated = 0U;
    data->sequence_started_ms = 0U;
}

static uint32_t movement_magnitude(int32_t value) {
    /* Avoid signed overflow even for INT32_MIN. */
    return value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
}

/* Unsigned subtraction remains correct across the 32-bit uptime wrap. */
static uint32_t elapsed_ms(uint32_t now, uint32_t then) {
    return now - then;
}

static uint32_t next_timeout_check_delay(uint32_t timeout_ms,
                                         uint32_t elapsed) {
    if (timeout_ms == 0U || elapsed >= timeout_ms) {
        return TIMEOUT_CHECK_INTERVAL_MS;
    }

    const uint32_t remaining = timeout_ms - elapsed;
    return remaining < TIMEOUT_CHECK_INTERVAL_MS ? remaining
                                                 : TIMEOUT_CHECK_INTERVAL_MS;
}

static void timeout_check_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, timeout_check_work);

    const int32_t layer = atomic_get(&data->active_layer);
    if (layer < 0) {
        return;
    }

    const uint32_t timeout_ms = data->timeout_ms;
    const uint32_t now = k_uptime_get_32();
    const uint32_t last_motion = (uint32_t)atomic_get(&data->last_motion_ms);
    const uint32_t elapsed = elapsed_ms(now, last_motion);

    if (timeout_ms > 0U && elapsed >= timeout_ms) {
        /*
         * Re-read immediately before deactivation. If movement arrived while
         * this work item was running, keep the layer and check again later.
         */
        const uint32_t newest_motion =
            (uint32_t)atomic_get(&data->last_motion_ms);

        if (newest_motion != last_motion ||
            elapsed_ms(k_uptime_get_32(), newest_motion) < timeout_ms) {
            k_work_reschedule(&data->timeout_check_work,
                              K_MSEC(TIMEOUT_CHECK_INTERVAL_MS));
            return;
        }

        if (!atomic_cas(&data->active_layer, layer, -1)) {
            k_work_reschedule(&data->timeout_check_work,
                              K_MSEC(TIMEOUT_CHECK_INTERVAL_MS));
            return;
        }

        data->timeout_ms = 0U;
        atomic_set(&data->last_motion_ms, 0);
        reset_pending_sequence(data);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        zmk_keymap_layer_deactivate((uint8_t)layer);
#endif
        return;
    }

    k_work_reschedule(
        &data->timeout_check_work,
        K_MSEC(next_timeout_check_delay(timeout_ms, elapsed)));
}

static int threshold_layer_handle_event(const struct device *dev,
                                        struct input_event *event,
                                        uint32_t param1,
                                        uint32_t param2,
                                        struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);

    struct threshold_layer_data *data = dev->data;
    const struct threshold_layer_config *config = dev->config;
    const uint8_t layer = (uint8_t)param1;
    const uint32_t timeout_ms = param2;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) ||
        event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const uint32_t now = k_uptime_get_32();

    /* Hot path while active: one atomic timestamp write, no work reschedule. */
    if (atomic_get(&data->active_layer) == (atomic_val_t)layer) {
        atomic_set(&data->last_motion_ms, (atomic_val_t)now);
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const uint32_t previous_motion =
        (uint32_t)atomic_get(&data->last_motion_ms);

    /* A movement gap starts a new threshold accumulation sequence. */
    if (previous_motion == 0U ||
        (config->movement_gap_ms > 0U &&
         elapsed_ms(now, previous_motion) > config->movement_gap_ms)) {
        reset_pending_sequence(data);
        data->sequence_started_ms = now;
    } else if (data->sequence_started_ms == 0U) {
        data->sequence_started_ms = now;
    }

    atomic_set(&data->last_motion_ms, (atomic_val_t)now);

    const uint32_t magnitude = movement_magnitude(event->value);
    if (UINT32_MAX - data->accumulated < magnitude) {
        data->accumulated = UINT32_MAX;
    } else {
        data->accumulated += magnitude;
    }

    const bool threshold_met = data->accumulated >= config->threshold;
    const bool delay_met =
        config->activation_delay_ms == 0U ||
        elapsed_ms(now, data->sequence_started_ms) >= config->activation_delay_ms;

    if (!threshold_met || !delay_met) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    reset_pending_sequence(data);
    data->timeout_ms = timeout_ms;
    atomic_set(&data->last_motion_ms, (atomic_val_t)now);
    atomic_set(&data->active_layer, (atomic_val_t)layer);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zmk_keymap_layer_activate(layer);

    if (timeout_ms > 0U) {
        k_work_reschedule(&data->timeout_check_work,
                          K_MSEC(timeout_ms < TIMEOUT_CHECK_INTERVAL_MS
                                     ? timeout_ms
                                     : TIMEOUT_CHECK_INTERVAL_MS));
    }
#else
    /* Split peripherals do not link the keymap layer implementation. */
    atomic_set(&data->active_layer, -1);
    data->timeout_ms = 0U;
#endif

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api threshold_layer_api = {
    .handle_event = threshold_layer_handle_event,
};

#define THRESHOLD_LAYER_INST(n)                                                    \
    static const struct threshold_layer_config threshold_layer_config_##n = {      \
        .threshold = DT_INST_PROP_OR(n, threshold, 128),                           \
        .movement_gap_ms = DT_INST_PROP_OR(n, movement_gap_ms, 100),               \
        .activation_delay_ms = DT_INST_PROP_OR(n, activation_delay_ms, 0),         \
    };                                                                              \
    static struct threshold_layer_data threshold_layer_data_##n = {                 \
        .active_layer = ATOMIC_INIT(-1),                                            \
        .last_motion_ms = ATOMIC_INIT(0),                                           \
    };                                                                              \
    static int threshold_layer_init_##n(const struct device *dev) {                 \
        struct threshold_layer_data *data = dev->data;                              \
        k_work_init_delayable(&data->timeout_check_work, timeout_check_handler);     \
        return 0;                                                                    \
    }                                                                                \
    DEVICE_DT_INST_DEFINE(n, threshold_layer_init_##n, NULL,                         \
                          &threshold_layer_data_##n, &threshold_layer_config_##n,     \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &threshold_layer_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_INST)
