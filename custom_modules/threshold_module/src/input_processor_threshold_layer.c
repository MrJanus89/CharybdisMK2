/*
 * Threshold + movement-gap + activation-delay temporary layer processor
 * for ZMK v0.3.x.
 *
 * param1: target layer
 * param2: inactivity timeout while the layer is active, in milliseconds
 *
 * Pointer events are observed only. They are never modified or consumed.
 */
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer

struct threshold_layer_config {
    uint32_t threshold;
    uint32_t movement_gap_ms;
    uint32_t activation_delay_ms;
};

struct threshold_layer_data {
    uint32_t accumulated;
    int16_t active_layer;

    int64_t sequence_started_ms;
    int64_t last_motion_ms;
    uint32_t active_timeout_ms;

    struct k_work_delayable deactivate_work;
};

static void reset_pending_sequence(struct threshold_layer_data *data) {
    data->accumulated = 0U;
    data->sequence_started_ms = 0;
}

static uint32_t movement_magnitude(int32_t value) {
    /* Avoid signed overflow even for INT32_MIN. */
    return value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
}

static void deactivate_layer(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, deactivate_work);

    if (data->active_layer < 0) {
        reset_pending_sequence(data);
        return;
    }

    const int64_t now = k_uptime_get();
    const int64_t idle_ms = now - data->last_motion_ms;

    /*
     * Movement while active only updates last_motion_ms. This work item checks
     * the newest timestamp and schedules itself for the remaining idle time.
     * Therefore, input events do not cancel/reschedule work continuously.
     */
    if (idle_ms < (int64_t)data->active_timeout_ms) {
        const uint32_t remaining_ms =
            (uint32_t)((int64_t)data->active_timeout_ms - idle_ms);
        k_work_schedule(&data->deactivate_work, K_MSEC(remaining_ms));
        return;
    }

    const int16_t layer = data->active_layer;
    data->active_layer = -1;
    reset_pending_sequence(data);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zmk_keymap_layer_deactivate((uint8_t)layer);
#endif
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

    const int64_t now = k_uptime_get();

    /* Fast path while active: only remember the newest movement time. */
    if (data->active_layer == layer) {
        data->last_motion_ms = now;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /*
     * A gap ends the previous movement sequence. The current event becomes
     * the first event of a new sequence.
     */
    if (data->last_motion_ms == 0 ||
        (config->movement_gap_ms > 0U &&
         now - data->last_motion_ms > (int64_t)config->movement_gap_ms)) {
        reset_pending_sequence(data);
        data->sequence_started_ms = now;
    } else if (data->sequence_started_ms == 0) {
        data->sequence_started_ms = now;
    }

    data->last_motion_ms = now;

    const uint32_t magnitude = movement_magnitude(event->value);
    if (UINT32_MAX - data->accumulated < magnitude) {
        data->accumulated = UINT32_MAX;
    } else {
        data->accumulated += magnitude;
    }

    const bool threshold_met = data->accumulated >= config->threshold;
    const bool delay_met =
        config->activation_delay_ms == 0U ||
        now - data->sequence_started_ms >= (int64_t)config->activation_delay_ms;

    if (!threshold_met || !delay_met) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    reset_pending_sequence(data);
    data->active_layer = layer;
    data->last_motion_ms = now;
    data->active_timeout_ms = timeout_ms;

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zmk_keymap_layer_activate(layer);

    /* Schedule once. Continued movement only updates last_motion_ms. */
    k_work_schedule(&data->deactivate_work, K_MSEC(timeout_ms));
#else
    /* Split peripherals do not link the keymap layer implementation. */
    data->active_layer = -1;
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
        .active_layer = -1,                                                         \
    };                                                                              \
    static int threshold_layer_init_##n(const struct device *dev) {                 \
        struct threshold_layer_data *data = dev->data;                              \
        k_work_init_delayable(&data->deactivate_work, deactivate_layer);            \
        return 0;                                                                    \
    }                                                                                \
    DEVICE_DT_INST_DEFINE(n, threshold_layer_init_##n, NULL,                         \
                          &threshold_layer_data_##n, &threshold_layer_config_##n,     \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &threshold_layer_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_INST)
