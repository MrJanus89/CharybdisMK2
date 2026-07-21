#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <errno.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#define DT_DRV_COMPAT zmk_behavior_mouse_layer_off

struct behavior_mouse_layer_off_config {
    uint8_t layer;
};

extern int zmk_threshold_layer_force_deactivate(uint8_t layer);

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    ARG_UNUSED(event);

    if (dev == NULL) {
        return -ENODEV;
    }

    const struct behavior_mouse_layer_off_config *config = dev->config;
    const int ret = zmk_threshold_layer_force_deactivate(config->layer);
    return ret < 0 ? ret : ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_mouse_layer_off_driver_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

#define MOUSE_LAYER_OFF_INST(n)                                                   \
    static const struct behavior_mouse_layer_off_config config_##n = {            \
        .layer = DT_INST_PROP(n, layer),                                           \
    };                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &config_##n, APPLICATION,         \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                     \
                          &behavior_mouse_layer_off_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MOUSE_LAYER_OFF_INST)
