#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer

struct threshold_layer_config {
    uint32_t threshold;
    uint32_t activation_delay_ms;
    uint32_t movement_gap_ms;
};

struct threshold_layer_data {
    int32_t accumulated;
    int16_t active_layer;
    int64_t movement_started_at;
    int64_t last_movement_at;
    struct k_work_delayable deactivate_work;
};

static void reset_pending_movement(struct threshold_layer_data *data) {
    data->accumulated = 0;
    data->movement_started_at = 0;
    data->last_movement_at = 0;
}

static void deactivate_layer(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, deactivate_work);

    if (data->active_layer >= 0) {
        zmk_keymap_layer_deactivate((uint8_t)data->active_layer);
        data->active_layer = -1;
    }

    reset_pending_movement(data);
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
    const uint32_t layer_timeout_ms = param2;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) ||
        event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();

    /*
     * Once active, every movement refreshes the layer timeout.
     */
    if (data->active_layer == layer) {
        data->last_movement_at = now;
        k_work_reschedule(&data->deactivate_work, K_MSEC(layer_timeout_ms));
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /*
     * Start a new continuity window when this is the first movement,
     * or when movement was interrupted for longer than movement-gap-ms.
     */
    if (data->movement_started_at == 0 ||
        data->last_movement_at == 0 ||
        (now - data->last_movement_at) > config->movement_gap_ms) {
        data->movement_started_at = now;
        data->accumulated = 0;
    }

    data->last_movement_at = now;
    data->accumulated += abs(event->value);

    const bool enough_time =
        (now - data->movement_started_at) >= config->activation_delay_ms;
    const bool enough_distance =
        (uint32_t)data->accumulated >= config->threshold;

    if (enough_time && enough_distance) {
        data->active_layer = layer;
        reset_pending_movement(data);
        zmk_keymap_layer_activate(layer);
        k_work_reschedule(&data->deactivate_work, K_MSEC(layer_timeout_ms));
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api threshold_layer_api = {
    .handle_event = threshold_layer_handle_event,
};

#define THRESHOLD_LAYER_INST(n)                                                    \
    static const struct threshold_layer_config threshold_layer_config_##n = {      \
        .threshold = DT_INST_PROP_OR(n, threshold, 16),                            \
        .activation_delay_ms = DT_INST_PROP_OR(n, activation_delay_ms, 300),       \
        .movement_gap_ms = DT_INST_PROP_OR(n, movement_gap_ms, 80),                \
    };                                                                              \
    static struct threshold_layer_data threshold_layer_data_##n = {                 \
        .active_layer = -1,                                                         \
    };                                                                              \
    static int threshold_layer_init_##n(const struct device *dev) {                 \
        struct threshold_layer_data *data = dev->data;                              \
        k_work_init_delayable(&data->deactivate_work, deactivate_layer);             \
        reset_pending_movement(data);                                                \
        return 0;                                                                    \
    }                                                                                \
    DEVICE_DT_INST_DEFINE(n, threshold_layer_init_##n, NULL,                         \
                          &threshold_layer_data_##n, &threshold_layer_config_##n,     \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &threshold_layer_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_INST)
