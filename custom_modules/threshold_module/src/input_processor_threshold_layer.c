/*
 * Low-latency threshold temporary-layer input processor for ZMK v0.3.x.
 *
 * param1: target layer
 * param2: inactivity timeout in milliseconds
 *
 * Input callback hot path:
 *   1. accumulate movement
 *   2. increment movement generation
 *   3. continue the original pointer event
 *
 * A separate periodic worker handles movement-gap, threshold,
 * activation-delay, layer activation, and inactivity timeout.
 */
#include <stdint.h>
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zmk_input_processor_threshold_layer
#define PROCESS_INTERVAL_MS 8U

struct threshold_layer_config {
    uint32_t threshold;
    uint32_t movement_gap_ms;
    uint32_t activation_delay_ms;
};

struct threshold_layer_data {
    const struct threshold_layer_config *config;

    /* Written by input callback, consumed by process_work. */
    atomic_t pending_movement;
    atomic_t motion_generation;
    atomic_t requested_layer;
    atomic_t requested_timeout_ms;
    /* Timestamp written directly by the pointer callback for race-free timeout checks. */
    atomic_t latest_motion_cycle;

    /* Owned by process_work only. */
    uint32_t last_seen_generation;
    uint32_t accumulated;
    uint32_t sequence_started_ms;
    uint32_t last_motion_ms;
    uint32_t timeout_ms;
    int16_t active_layer;

    /* Set by mouse_off. Blocks reactivation until the pointer has been idle. */
    bool manual_reactivation_block;
    uint32_t manual_block_last_motion_ms;

    struct k_work_delayable process_work;
};

static uint32_t movement_magnitude(int32_t value) {
    return value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
}

static uint32_t elapsed_ms(uint32_t now, uint32_t then) {
    return now - then;
}

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

static void process_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct threshold_layer_data *data =
        CONTAINER_OF(dwork, struct threshold_layer_data, process_work);
    const struct threshold_layer_config *config = data->config;

    const uint32_t now = k_uptime_get_32();
    const uint32_t generation =
        (uint32_t)atomic_get(&data->motion_generation);
    const bool moved = generation != data->last_seen_generation;

    if (moved) {
        const uint32_t movement =
            (uint32_t)atomic_set(&data->pending_movement, 0);
        const uint32_t previous_motion_ms = data->last_motion_ms;

        data->last_seen_generation = generation;
        data->last_motion_ms = now;

        if (data->manual_reactivation_block) {
            /* Ignore all residual motion after mouse_off. */
            data->manual_block_last_motion_ms = now;
            reset_sequence(data);
        } else if (data->active_layer < 0) {
            if (previous_motion_ms == 0U ||
                (config->movement_gap_ms > 0U &&
                 elapsed_ms(now, previous_motion_ms) >
                     config->movement_gap_ms)) {
                reset_sequence(data);
                data->sequence_started_ms = now;
            } else if (data->sequence_started_ms == 0U) {
                data->sequence_started_ms = now;
            }

            saturating_add(&data->accumulated, movement);
        }
    }

    if (data->manual_reactivation_block) {
        const uint32_t release_gap =
            config->movement_gap_ms > 0U ? config->movement_gap_ms : 50U;

        if (!moved && data->manual_block_last_motion_ms != 0U &&
            elapsed_ms(now, data->manual_block_last_motion_ms) >= release_gap) {
            data->manual_reactivation_block = false;
            data->manual_block_last_motion_ms = 0U;
            data->last_motion_ms = 0U;
            reset_sequence(data);
            atomic_set(&data->pending_movement, 0);
        }
    }

    if (!data->manual_reactivation_block && data->active_layer < 0 &&
        data->sequence_started_ms != 0U) {
        const bool threshold_met = data->accumulated >= config->threshold;
        const bool delay_met =
            config->activation_delay_ms == 0U ||
            elapsed_ms(now, data->sequence_started_ms) >=
                config->activation_delay_ms;

        if (threshold_met && delay_met) {
            const int16_t layer =
                (int16_t)atomic_get(&data->requested_layer);
            const uint32_t timeout_ms =
                (uint32_t)atomic_get(&data->requested_timeout_ms);

            reset_sequence(data);
            data->active_layer = layer;
            data->timeout_ms = timeout_ms;

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
            zmk_keymap_layer_activate((uint8_t)layer);
#else
            data->active_layer = -1;
            data->timeout_ms = 0U;
#endif
        }
    }

    if (data->active_layer >= 0 && data->timeout_ms > 0U) {
        const uint32_t now_cycle = k_cycle_get_32();
        const uint32_t motion_cycle =
            (uint32_t)atomic_get(&data->latest_motion_cycle);

        if (motion_cycle != 0U &&
            k_cyc_to_ms_floor32(now_cycle - motion_cycle) >= data->timeout_ms) {
            /*
             * Re-read immediately before closing. If an input event arrived
             * during the timeout calculation, keep the layer active.
             */
            const uint32_t confirmed_motion_cycle =
                (uint32_t)atomic_get(&data->latest_motion_cycle);

            if (confirmed_motion_cycle == motion_cycle &&
                k_cyc_to_ms_floor32(k_cycle_get_32() - confirmed_motion_cycle) >=
                    data->timeout_ms) {
                const int16_t layer = data->active_layer;

                data->active_layer = -1;
                data->timeout_ms = 0U;
                data->last_motion_ms = 0U;
                reset_sequence(data);
                atomic_set(&data->pending_movement, 0);
                atomic_set(&data->latest_motion_cycle, 0);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
                zmk_keymap_layer_deactivate((uint8_t)layer);
#endif
            }
        }
    }

    k_work_reschedule(&data->process_work, K_MSEC(PROCESS_INTERVAL_MS));
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

    /* Keep the pointer callback independent from all layer decisions. */
    atomic_set(&data->requested_layer, (atomic_val_t)param1);
    atomic_set(&data->requested_timeout_ms, (atomic_val_t)param2);
    atomic_add(&data->pending_movement,
               (atomic_val_t)movement_magnitude(event->value));
    atomic_set(&data->latest_motion_cycle, (atomic_val_t)k_cycle_get_32());
    atomic_inc(&data->motion_generation);

    return ZMK_INPUT_PROC_CONTINUE;
}


static struct threshold_layer_data *primary_threshold_layer_data;

int zmk_threshold_layer_force_deactivate(uint8_t layer) {
    struct threshold_layer_data *data = primary_threshold_layer_data;

    if (data == NULL) {
        return -ENODEV;
    }

    /* Drop accumulated movement and block immediate reactivation caused by
     * residual events from the same physical trackball motion. */
    data->active_layer = -1;
    data->timeout_ms = 0U;
    data->last_motion_ms = 0U;
    data->manual_reactivation_block = true;
    data->manual_block_last_motion_ms = k_uptime_get_32();
    reset_sequence(data);
    atomic_set(&data->pending_movement, 0);
    atomic_set(&data->latest_motion_cycle, 0);
    data->last_seen_generation =
        (uint32_t)atomic_get(&data->motion_generation);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    return zmk_keymap_layer_deactivate(layer);
#else
    return 0;
#endif
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
        .requested_layer = ATOMIC_INIT(0),                                          \
        .requested_timeout_ms = ATOMIC_INIT(0),                                     \
        .latest_motion_cycle = ATOMIC_INIT(0),                                      \
        .manual_reactivation_block = false,                                         \
        .manual_block_last_motion_ms = 0U,                                           \
    };                                                                              \
    static int threshold_layer_init_##n(const struct device *dev) {                 \
        struct threshold_layer_data *data = dev->data;                              \
        data->config = dev->config;                                                  \
        if (primary_threshold_layer_data == NULL) {                                  \
            primary_threshold_layer_data = data;                                     \
        }                                                                             \
        k_work_init_delayable(&data->process_work, process_handler);                 \
        k_work_schedule(&data->process_work, K_MSEC(PROCESS_INTERVAL_MS));           \
        return 0;                                                                    \
    }                                                                                \
    DEVICE_DT_INST_DEFINE(n, threshold_layer_init_##n, NULL,                         \
                          &threshold_layer_data_##n, &threshold_layer_config_##n,     \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &threshold_layer_api);

DT_INST_FOREACH_STATUS_OKAY(THRESHOLD_LAYER_INST)
