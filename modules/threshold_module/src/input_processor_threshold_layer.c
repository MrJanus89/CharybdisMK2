/* Movement-threshold temporary layer input processor for ZMK v0.3.x. */
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer

struct threshold_layer_config {
    uint32_t threshold;
};

struct threshold_layer_data {
    int32_t accumulated;
    int16_t active_layer;
    struct k_work_delayable deactivate_work;
};

static void deactivate_layer(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, deactivate_work);

    if (data->active_layer >= 0) {
        zmk_keymap_layer_deactivate((uint8_t)data->active_layer);
        data->active_layer = -1;
    }

    data->accumulated = 0;
}

static int threshold_layer_handle_event(const struct device *dev,
                                        struct input_event *event,
                                        uint32_t param1,
                                        uint32_t param2,
                                        struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);

    const struct threshold_layer_config *cfg = dev->config;
    struct threshold_layer_data *data = dev->data;
    const uint8_t layer = (uint8_t)param1;
    const uint32_t timeout_ms = param2;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (data->active_layer == layer) {
        k_work_reschedule(&data->deactivate_work, K_MSEC(timeout_ms));
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->accumulated += abs(event->value);

    if ((uint32_t)data->accumulated >= cfg->threshold) {
        data->accumulated = 0;
        data->active_layer = layer;
        zmk_keymap_layer_activate(layer);
        k_work_reschedule(&data->deactivate_work, K_MSEC(timeout_ms));
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api threshold_layer_api = {
    .handle_event = threshold_layer_handle_event,
};

#define THRESHOLD_LAYER_INST(n)                                                            \
    static struct threshold_layer_data threshold_layer_data_##n = {.active_layer = -1};   \
    static const struct threshold_layer_config threshold_layer_config_##n = {              \
        .threshold = DT_INST_PROP(n, threshold),                                            \
    };                                                                                      \
    static int threshold_layer_init_##n(const struct device *dev) {                        \
        struct threshold_layer_data *data = dev->data;                                      \
        k_work_init_delayable(&data->deactivate_work, deactivate_layer);                    \
        return 0;                                                                           \
    }                                                                                       \
    DEVICE_DT_INST_DEFINE(n, threshold_layer_init_##n, NULL,                                \
                          &threshold_layer_data_##n, &threshold_layer_config_##n,            \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                  \
                          &threshold_layer_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_INST)
