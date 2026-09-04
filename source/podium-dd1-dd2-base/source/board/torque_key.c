#include "board/torque_key.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Torque Key debounce timing constants.
 *
 * The integrator traverses this duration between stable removed and inserted states.
 */
enum {
    TORQUE_KEY_FILTER_MS = 500, /**< Full integrator travel in milliseconds. */
    TORQUE_KEY_INITIAL_POSITION_MS = TORQUE_KEY_FILTER_MS / 2, /**< Neutral startup position. */
};

/**
 * @brief Initializes Torque Key transition filtering.
 *
 * Starts at the midpoint of the 500-millisecond integrator without a reported key state. Either
 * initial state must remain dominant for 250 milliseconds before it is reported.
 *
 * @param[out] key Torque Key filter to initialize.
 */
void torque_key_init(TorqueKey *key) {
    *key = (TorqueKey){.filter_position_ms = TORQUE_KEY_INITIAL_POSITION_MS};
}

/**
 * @brief Filters the logical Torque Key state into stable transitions.
 *
 * Checks an endpoint before integrating the current sample, backs the integrator away by one
 * millisecond, and then integrates the current sample.
 * The neutral startup position requires 250 milliseconds to establish either initial state. Later
 * state changes require 500 milliseconds of net travel, with opposite samples cancelling prior
 * travel.
 *
 * @param[in,out] key Torque Key filter state.
 * @param[in] raw_inserted True when the logical Torque Key input reports insertion.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void torque_key_update(TorqueKey *key, bool raw_inserted, uint32_t now_ms) {
    if (!key->initialized) {
        key->initialized = true;
        key->last_update_ms = now_ms;
        return;
    }

    uint32_t elapsed_ms = now_ms - key->last_update_ms;
    key->last_update_ms = now_ms;
    if (elapsed_ms > TORQUE_KEY_FILTER_MS) {
        elapsed_ms = TORQUE_KEY_FILTER_MS;
    }

    if (key->filter_position_ms == TORQUE_KEY_FILTER_MS) {
        if (!key->state_known || key->inserted) {
            key->state_known = true;
            key->inserted = false;
        }
        key->filter_position_ms = TORQUE_KEY_FILTER_MS - 1;
    } else if (key->filter_position_ms == 0) {
        if (!key->state_known || !key->inserted) {
            key->state_known = true;
            key->inserted = true;
        }
        key->filter_position_ms = 1;
    }

    if (raw_inserted) {
        key->filter_position_ms = elapsed_ms >= key->filter_position_ms
                                      ? 0
                                      : (uint16_t)(key->filter_position_ms - elapsed_ms);
    } else {
        uint32_t position_ms = key->filter_position_ms + elapsed_ms;
        key->filter_position_ms =
            position_ms >= TORQUE_KEY_FILTER_MS ? TORQUE_KEY_FILTER_MS : (uint16_t)position_ms;
    }
}
