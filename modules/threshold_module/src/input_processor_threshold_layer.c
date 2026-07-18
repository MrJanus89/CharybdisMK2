/*
 * Movement-threshold temporary layer input processor for ZMK v0.3.
 *
 * Runtime parameters:
 *   param1: layer
 *   param2: timeout in milliseconds
 *
 * Per-instance Devicetree property:
 *   movement-threshold: accumulated absolute X/Y movement before activation
 */

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer

#include <stdint.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

struct threshold_layer_config {
    uint32_t movement_threshold;
};

struct threshold_layer_data {
    uint32_t accumulated_movement;
    int16_t active_layer;
    struct k_work_delayable deactivate_work;
};

static void threshold_layer_deactivate(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(delayable, struct threshold_layer_data, deactivate_work);

    if (data->active_layer >= 0) {
        zmk_keymap_layer_deactivate((uint8_t)data->active_layer);
        data->active_layer = -1;
    }

    data->accumulated_movement = 0U;
}

static int threshold_layer_handle_event(const struct device *dev, struct input_event *event,
                                        uint32_t param1, uint32_t param2,
                                        struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);

    const struct threshold_layer_config *config = dev->config;
    struct threshold_layer_data *data = dev->data;
    const uint8_t requested_layer = (uint8_t)param1;
    const uint32_t timeout_ms = param2;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* Once active, every X/Y event refreshes the temporary-layer timeout. */
    if (data->active_layer == requested_layer) {
        k_work_reschedule(&data->deactivate_work, K_MSEC(timeout_ms));
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* If this instance was previously used for a different layer, release it first. */
    if (data->active_layer >= 0) {
        zmk_keymap_layer_deactivate((uint8_t)data->active_layer);
        data->active_layer = -1;
        (void)k_work_cancel_delayable(&data->deactivate_work);
    }

    const int32_t value = event->value;
    const uint32_t magnitude = (value < 0) ? (uint32_t)(-value) : (uint32_t)value;

    if (UINT32_MAX - data->accumulated_movement < magnitude) {
        data->accumulated_movement = config->movement_threshold;
    } else {
        data->accumulated_movement += magnitude;
    }

    if (data->accumulated_movement < config->movement_threshold) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->accumulated_movement = 0U;
    data->active_layer = requested_layer;
    zmk_keymap_layer_activate(requested_layer);
    k_work_reschedule(&data->deactivate_work, K_MSEC(timeout_ms));

    return ZMK_INPUT_PROC_CONTINUE;
}

static int threshold_layer_init(const struct device *dev) {
    struct threshold_layer_data *data = dev->data;

    data->accumulated_movement = 0U;
    data->active_layer = -1;
    k_work_init_delayable(&data->deactivate_work, threshold_layer_deactivate);

    return 0;
}

static const struct zmk_input_processor_driver_api threshold_layer_driver_api = {
    .handle_event = threshold_layer_handle_event,
};

#define THRESHOLD_LAYER_DEFINE(inst)                                                       \
    static const struct threshold_layer_config threshold_layer_config_##inst = {           \
        .movement_threshold = DT_INST_PROP(inst, movement_threshold),                      \
    };                                                                                     \
    static struct threshold_layer_data threshold_layer_data_##inst;                        \
    DEVICE_DT_INST_DEFINE(inst, threshold_layer_init, NULL, &threshold_layer_data_##inst,  \
                          &threshold_layer_config_##inst, POST_KERNEL,                      \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                              \
                          &threshold_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_DEFINE)
