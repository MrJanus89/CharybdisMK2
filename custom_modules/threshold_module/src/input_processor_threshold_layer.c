/*
 * Movement-threshold temporary layer input processor for ZMK v0.3.x.
 *
 * Response-oriented implementation:
 * - Pointer events are never modified or consumed.
 * - Active movement only updates last_motion_ms.
 * - A delayed work item checks inactivity and reschedules itself only when needed.
 * - No work cancellation/rescheduling occurs for every X/Y event.
 */
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer

/* Raw absolute X/Y movement required before the temporary layer activates. */
#define THRESHOLD_MOVEMENT_UNITS 32U

struct threshold_layer_data {
    int32_t accumulated;
    int16_t active_layer;
    int64_t last_motion_ms;
    uint32_t timeout_ms;
    struct k_work_delayable deactivate_work;
};

static void deactivate_layer(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, deactivate_work);

    if (data->active_layer < 0) {
        data->accumulated = 0;
        return;
    }

    const int64_t now = k_uptime_get();
    const int64_t idle_ms = now - data->last_motion_ms;

    /*
     * Movement may have occurred since this work item was originally scheduled.
     * Do not cancel/requeue work from each input event. Instead, check the latest
     * timestamp here and sleep only for the remaining inactivity period.
     */
    if (idle_ms < (int64_t)data->timeout_ms) {
        const uint32_t remaining_ms = (uint32_t)((int64_t)data->timeout_ms - idle_ms);
        k_work_schedule(&data->deactivate_work, K_MSEC(remaining_ms));
        return;
    }

    const int16_t layer = data->active_layer;
    data->active_layer = -1;
    data->accumulated = 0;

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
    const uint8_t layer = (uint8_t)param1;
    const uint32_t timeout_ms = param2;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();

    /*
     * Fast path while active: record movement time only.
     * No k_work_reschedule(), no event modification, and no event suppression.
     */
    if (data->active_layer == layer) {
        data->last_motion_ms = now;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->accumulated += abs(event->value);

    if ((uint32_t)data->accumulated >= THRESHOLD_MOVEMENT_UNITS) {
        data->accumulated = 0;
        data->active_layer = layer;
        data->last_motion_ms = now;
        data->timeout_ms = timeout_ms;

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        zmk_keymap_layer_activate(layer);

        /* One initial schedule. Further movement only updates last_motion_ms. */
        k_work_schedule(&data->deactivate_work, K_MSEC(timeout_ms));
#else
        /* Split peripherals do not link the keymap layer implementation. */
        data->active_layer = -1;
#endif
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api threshold_layer_api = {
    .handle_event = threshold_layer_handle_event,
};

#define THRESHOLD_LAYER_INST(n)                                                          \
    static struct threshold_layer_data threshold_layer_data_##n = {                     \
        .active_layer = -1,                                                              \
    };                                                                                   \
    static int threshold_layer_init_##n(const struct device *dev) {                     \
        struct threshold_layer_data *data = dev->data;                                  \
        k_work_init_delayable(&data->deactivate_work, deactivate_layer);                \
        return 0;                                                                        \
    }                                                                                    \
    DEVICE_DT_INST_DEFINE(n, threshold_layer_init_##n, NULL,                            \
                          &threshold_layer_data_##n, NULL,                               \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,              \
                          &threshold_layer_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_INST)
