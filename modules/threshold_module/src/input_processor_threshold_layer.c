/*
 * Movement-threshold temporary layer input processor for ZMK v0.3.
 *
 * All settings are Devicetree properties. The processor takes zero phandle
 * parameters, avoiding version-specific input-processor cell merging.
 */

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

struct threshold_layer_config {
    uint8_t layer;
    uint32_t timeout_ms;
    uint32_t movement_threshold;
};

struct threshold_layer_data {
    uint32_t accumulated_movement;
    bool layer_active;
    const struct device *dev;
    struct k_work_delayable deactivate_work;
};

static void threshold_layer_deactivate(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(delayable, struct threshold_layer_data, deactivate_work);
    const struct threshold_layer_config *config = data->dev->config;

    if (data->layer_active) {
        zmk_keymap_layer_deactivate(config->layer);
        data->layer_active = false;
    }

    data->accumulated_movement = 0U;
}

static int threshold_layer_handle_event(const struct device *dev, struct input_event *event,
                                        uint32_t param1, uint32_t param2,
                                        struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct threshold_layer_config *config = dev->config;
    struct threshold_layer_data *data = dev->data;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (data->layer_active) {
        k_work_reschedule(&data->deactivate_work, K_MSEC(config->timeout_ms));
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t value = event->value;
    uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;

    if (UINT32_MAX - data->accumulated_movement < magnitude) {
        data->accumulated_movement = config->movement_threshold;
    } else {
        data->accumulated_movement += magnitude;
    }

    if (data->accumulated_movement >= config->movement_threshold) {
        data->accumulated_movement = 0U;
        data->layer_active = true;
        zmk_keymap_layer_activate(config->layer);
        k_work_reschedule(&data->deactivate_work, K_MSEC(config->timeout_ms));
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static int threshold_layer_init(const struct device *dev) {
    struct threshold_layer_data *data = dev->data;

    data->accumulated_movement = 0U;
    data->layer_active = false;
    data->dev = dev;
    k_work_init_delayable(&data->deactivate_work, threshold_layer_deactivate);

    return 0;
}

static const struct zmk_input_processor_driver_api threshold_layer_driver_api = {
    .handle_event = threshold_layer_handle_event,
};

#define THRESHOLD_LAYER_DEFINE(inst)                                                      \
    static const struct threshold_layer_config threshold_layer_config_##inst = {          \
        .layer = DT_INST_PROP(inst, layer),                                               \
        .timeout_ms = DT_INST_PROP(inst, timeout_ms),                                     \
        .movement_threshold = DT_INST_PROP(inst, movement_threshold),                     \
    };                                                                                    \
    static struct threshold_layer_data threshold_layer_data_##inst;                       \
    DEVICE_DT_INST_DEFINE(inst, threshold_layer_init, NULL, &threshold_layer_data_##inst, \
                          &threshold_layer_config_##inst, POST_KERNEL,                     \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                             \
                          &threshold_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_DEFINE)
