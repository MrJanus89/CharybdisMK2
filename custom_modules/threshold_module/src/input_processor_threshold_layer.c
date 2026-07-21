/*
 * Event-driven threshold temporary-layer input processor for ZMK v0.3.x.
 *
 * param1: target layer
 * param2: inactivity timeout in milliseconds
 *
 * Design:
 * - No permanently-running worker while idle.
 * - Before activation, the first movement schedules a short-lived activation worker.
 * - After activation, pointer events only refresh a cycle timestamp.
 * - A separate 100 ms timeout worker runs only while the layer is active.
 * - mouse_off atomically marks the layer inactive, cancels timeout handling,
 *   and blocks reactivation until the pointer has been idle for movement-gap-ms.
 */
#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer

#define ACTIVATION_INTERVAL_MS 8U
#define TIMEOUT_CHECK_INTERVAL_MS 100U
#define MANUAL_BLOCK_CHECK_INTERVAL_MS 20U

struct threshold_layer_config {
    uint32_t threshold;
    uint32_t movement_gap_ms;
    uint32_t activation_delay_ms;
};

struct threshold_layer_data {
    const struct threshold_layer_config *config;

    /* Input callback state. active_layer_plus_one == 0 means inactive. */
    atomic_t active_layer_plus_one;
    atomic_t pending_movement;
    atomic_t motion_generation;
    atomic_t latest_motion_cycle;
    atomic_t requested_layer;
    atomic_t requested_timeout_ms;
    atomic_t activation_work_armed;
    atomic_t manual_reactivation_block;

    /* Activation worker owned state. */
    uint32_t last_seen_generation;
    uint32_t accumulated;
    uint32_t sequence_started_ms;
    uint32_t last_motion_ms;
    uint32_t manual_block_last_motion_ms;

    /* Timeout worker state. */
    uint32_t timeout_ms;

    struct k_work_delayable activation_work;
    struct k_work_delayable timeout_work;
};

static struct threshold_layer_data *primary_threshold_layer_data;

static uint32_t movement_magnitude(int32_t value) {
    return value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
}

static uint32_t elapsed_ms(uint32_t now, uint32_t then) { return now - then; }

static void reset_sequence(struct threshold_layer_data *data) {
    data->accumulated = 0U;
    data->sequence_started_ms = 0U;
}

static void saturating_add(uint32_t *target, uint32_t value) {
    if (UINT32_MAX - *target < value) {
        *target = UINT32_MAX;
    } else {
        *target += value;
    }
}

static bool layer_is_active(const struct threshold_layer_data *data) {
    return atomic_get(&data->active_layer_plus_one) != 0;
}

static void schedule_activation_work(struct threshold_layer_data *data, uint32_t delay_ms) {
    if (atomic_cas(&data->activation_work_armed, 0, 1)) {
        k_work_reschedule(&data->activation_work, K_MSEC(delay_ms));
    }
}

/*
 * Release ownership of activation_work. If movement raced with the release,
 * immediately arm it again so no first movement is lost.
 */
static void disarm_activation_work(struct threshold_layer_data *data,
                                   uint32_t observed_generation) {
    atomic_set(&data->activation_work_armed, 0);

    if (!layer_is_active(data) &&
        ((uint32_t)atomic_get(&data->motion_generation) != observed_generation ||
         atomic_get(&data->manual_reactivation_block) != 0)) {
        schedule_activation_work(data, 0U);
    }
}

static void timeout_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, timeout_work);

    const atomic_val_t active_plus_one = atomic_get(&data->active_layer_plus_one);
    if (active_plus_one == 0 || data->timeout_ms == 0U) {
        return;
    }

    const uint32_t motion_cycle = (uint32_t)atomic_get(&data->latest_motion_cycle);
    if (motion_cycle == 0U ||
        k_cyc_to_ms_floor32(k_cycle_get_32() - motion_cycle) < data->timeout_ms) {
        k_work_reschedule(&data->timeout_work, K_MSEC(TIMEOUT_CHECK_INTERVAL_MS));
        return;
    }

    /* Confirm no pointer event arrived while calculating the timeout. */
    const uint32_t confirmed_cycle =
        (uint32_t)atomic_get(&data->latest_motion_cycle);
    if (confirmed_cycle != motion_cycle ||
        k_cyc_to_ms_floor32(k_cycle_get_32() - confirmed_cycle) < data->timeout_ms) {
        k_work_reschedule(&data->timeout_work, K_MSEC(TIMEOUT_CHECK_INTERVAL_MS));
        return;
    }

    /* Only this worker instance may close the layer it observed. */
    if (!atomic_cas(&data->active_layer_plus_one, active_plus_one, 0)) {
        return;
    }

    const uint8_t layer = (uint8_t)(active_plus_one - 1);
    data->timeout_ms = 0U;
    data->last_motion_ms = 0U;
    reset_sequence(data);
    atomic_set(&data->pending_movement, 0);
    atomic_set(&data->latest_motion_cycle, 0);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zmk_keymap_layer_deactivate(layer);
#endif
}

static void activation_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, activation_work);
    const struct threshold_layer_config *config = data->config;

    const uint32_t now = k_uptime_get_32();
    const uint32_t generation = (uint32_t)atomic_get(&data->motion_generation);
    const bool moved = generation != data->last_seen_generation;

    if (moved) {
        const uint32_t movement = (uint32_t)atomic_set(&data->pending_movement, 0);
        const uint32_t previous_motion_ms = data->last_motion_ms;

        data->last_seen_generation = generation;
        data->last_motion_ms = now;

        if (atomic_get(&data->manual_reactivation_block) != 0) {
            data->manual_block_last_motion_ms = now;
            reset_sequence(data);
        } else if (!layer_is_active(data)) {
            if (previous_motion_ms == 0U ||
                (config->movement_gap_ms > 0U &&
                 elapsed_ms(now, previous_motion_ms) > config->movement_gap_ms)) {
                reset_sequence(data);
                data->sequence_started_ms = now;
            } else if (data->sequence_started_ms == 0U) {
                data->sequence_started_ms = now;
            }

            saturating_add(&data->accumulated, movement);
        }
    }

    if (atomic_get(&data->manual_reactivation_block) != 0) {
        const uint32_t release_gap =
            config->movement_gap_ms > 0U ? config->movement_gap_ms : 50U;

        if (!moved && data->manual_block_last_motion_ms != 0U &&
            elapsed_ms(now, data->manual_block_last_motion_ms) >= release_gap) {
            atomic_set(&data->manual_reactivation_block, 0);
            data->manual_block_last_motion_ms = 0U;
            data->last_motion_ms = 0U;
            reset_sequence(data);
            atomic_set(&data->pending_movement, 0);
            disarm_activation_work(data, generation);
            return;
        }

        k_work_reschedule(&data->activation_work,
                          K_MSEC(MANUAL_BLOCK_CHECK_INTERVAL_MS));
        return;
    }

    if (layer_is_active(data)) {
        disarm_activation_work(data, generation);
        return;
    }

    if (data->sequence_started_ms != 0U) {
        const bool threshold_met = data->accumulated >= config->threshold;
        const bool delay_met =
            config->activation_delay_ms == 0U ||
            elapsed_ms(now, data->sequence_started_ms) >= config->activation_delay_ms;

        if (threshold_met && delay_met) {
            const uint8_t layer = (uint8_t)atomic_get(&data->requested_layer);
            const uint32_t timeout_ms =
                (uint32_t)atomic_get(&data->requested_timeout_ms);

            reset_sequence(data);
            data->timeout_ms = timeout_ms;
            atomic_set(&data->active_layer_plus_one, (atomic_val_t)layer + 1);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
            zmk_keymap_layer_activate(layer);
#else
            atomic_set(&data->active_layer_plus_one, 0);
            data->timeout_ms = 0U;
#endif

            if (layer_is_active(data) && timeout_ms > 0U) {
                k_work_reschedule(&data->timeout_work,
                                  K_MSEC(TIMEOUT_CHECK_INTERVAL_MS));
            }

            disarm_activation_work(data, generation);
            return;
        }

        /* Stop polling after the current movement sequence has gone idle. */
        const uint32_t gap = config->movement_gap_ms > 0U
                                 ? config->movement_gap_ms
                                 : ACTIVATION_INTERVAL_MS;
        if (!moved && data->last_motion_ms != 0U &&
            elapsed_ms(now, data->last_motion_ms) >= gap) {
            data->last_motion_ms = 0U;
            reset_sequence(data);
            atomic_set(&data->pending_movement, 0);
            disarm_activation_work(data, generation);
            return;
        }
    }

    k_work_reschedule(&data->activation_work, K_MSEC(ACTIVATION_INTERVAL_MS));
}

static int threshold_layer_handle_event(const struct device *dev,
                                        struct input_event *event,
                                        uint32_t param1,
                                        uint32_t param2,
                                        struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);

    struct threshold_layer_data *data = dev->data;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) ||
        event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    atomic_set(&data->requested_layer, (atomic_val_t)param1);
    atomic_set(&data->requested_timeout_ms, (atomic_val_t)param2);
    atomic_set(&data->latest_motion_cycle, (atomic_val_t)k_cycle_get_32());
    atomic_inc(&data->motion_generation);

    /* Once active, even one count of movement refreshes the inactivity timeout. */
    if (layer_is_active(data)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* Before activation, only movement accumulation and one worker arm are needed. */
    atomic_add(&data->pending_movement,
               (atomic_val_t)movement_magnitude(event->value));
    schedule_activation_work(data, 0U);

    return ZMK_INPUT_PROC_CONTINUE;
}

int zmk_threshold_layer_force_deactivate(uint8_t layer) {
    struct threshold_layer_data *data = primary_threshold_layer_data;

    if (data == NULL) {
        return -ENODEV;
    }

    /* Block first, then invalidate every pending automatic state transition. */
    atomic_set(&data->manual_reactivation_block, 1);
    atomic_set(&data->active_layer_plus_one, 0);
    k_work_cancel_delayable(&data->timeout_work);
    k_work_cancel_delayable(&data->activation_work);
    atomic_set(&data->activation_work_armed, 0);

    data->timeout_ms = 0U;
    data->last_motion_ms = 0U;
    data->manual_block_last_motion_ms = k_uptime_get_32();
    reset_sequence(data);
    atomic_set(&data->pending_movement, 0);
    atomic_set(&data->latest_motion_cycle, 0);
    data->last_seen_generation =
        (uint32_t)atomic_get(&data->motion_generation);

    /* Keep a lightweight worker alive only until the pointer becomes idle. */
    schedule_activation_work(data, MANUAL_BLOCK_CHECK_INTERVAL_MS);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    return zmk_keymap_layer_deactivate(layer);
#else
    return 0;
#endif
}

static const struct zmk_input_processor_driver_api threshold_layer_api = {
    .handle_event = threshold_layer_handle_event,
};

#define THRESHOLD_LAYER_INST(n)                                                   \
    static const struct threshold_layer_config threshold_layer_config_##n = {     \
        .threshold = DT_INST_PROP_OR(n, threshold, 128),                          \
        .movement_gap_ms = DT_INST_PROP_OR(n, movement_gap_ms, 100),              \
        .activation_delay_ms = DT_INST_PROP_OR(n, activation_delay_ms, 0),        \
    };                                                                             \
    static struct threshold_layer_data threshold_layer_data_##n = {                \
        .active_layer_plus_one = ATOMIC_INIT(0),                                   \
        .pending_movement = ATOMIC_INIT(0),                                        \
        .motion_generation = ATOMIC_INIT(0),                                       \
        .latest_motion_cycle = ATOMIC_INIT(0),                                     \
        .requested_layer = ATOMIC_INIT(0),                                         \
        .requested_timeout_ms = ATOMIC_INIT(0),                                    \
        .activation_work_armed = ATOMIC_INIT(0),                                   \
        .manual_reactivation_block = ATOMIC_INIT(0),                               \
    };                                                                             \
    static int threshold_layer_init_##n(const struct device *dev) {                \
        struct threshold_layer_data *data = dev->data;                             \
        data->config = dev->config;                                                 \
        if (primary_threshold_layer_data == NULL) {                                 \
            primary_threshold_layer_data = data;                                    \
        }                                                                            \
        k_work_init_delayable(&data->activation_work, activation_handler);          \
        k_work_init_delayable(&data->timeout_work, timeout_handler);                \
        return 0;                                                                    \
    }                                                                                \
    DEVICE_DT_INST_DEFINE(n, threshold_layer_init_##n, NULL,                        \
                          &threshold_layer_data_##n, &threshold_layer_config_##n,    \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,          \
                          &threshold_layer_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_INST)
