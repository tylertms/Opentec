#include "board/torque_key.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    TORQUE_KEY_FILTER_MS = 500,
    TORQUE_KEY_INITIAL_POSITION_MS = TORQUE_KEY_FILTER_MS / 2,
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
 * @brief Filters the active-low Torque Key state into stable transitions.
 *
 * Integrates elapsed milliseconds toward the inserted or removed endpoint. The neutral startup
 * position requires 250 milliseconds to establish either initial state. Later state changes
 * require 500 milliseconds of net travel, with opposite samples cancelling prior travel.
 *
 * @param[in,out] key Torque Key filter state.
 * @param[in] raw_inserted True while the physical Torque Key input is active.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Stable insertion or removal transition, or no transition.
 */
TorqueKeyEvent torque_key_update(TorqueKey *key, bool raw_inserted, uint32_t now_ms) {
    if (!key->initialized) {
        key->initialized = true;
        key->last_update_ms = now_ms;
        return TORQUE_KEY_EVENT_NONE;
    }

    uint32_t elapsed_ms = now_ms - key->last_update_ms;
    key->last_update_ms = now_ms;
    if (elapsed_ms > TORQUE_KEY_FILTER_MS) {
        elapsed_ms = TORQUE_KEY_FILTER_MS;
    }

    if (raw_inserted) {
        key->filter_position_ms = elapsed_ms >= key->filter_position_ms
                                      ? 0
                                      : (uint16_t)(key->filter_position_ms - elapsed_ms);
        if (key->filter_position_ms == 0 && (!key->state_known || !key->inserted)) {
            key->state_known = true;
            key->inserted = true;
            return TORQUE_KEY_EVENT_INSERTED;
        }
    } else {
        uint32_t position_ms = key->filter_position_ms + elapsed_ms;
        key->filter_position_ms =
            position_ms >= TORQUE_KEY_FILTER_MS ? TORQUE_KEY_FILTER_MS : (uint16_t)position_ms;
        if (key->filter_position_ms == TORQUE_KEY_FILTER_MS &&
            (!key->state_known || key->inserted)) {
            key->state_known = true;
            key->inserted = false;
            return TORQUE_KEY_EVENT_REMOVED;
        }
    }
    return TORQUE_KEY_EVENT_NONE;
}
