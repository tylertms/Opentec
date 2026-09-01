#ifndef OPENTEC_BASE_BOARD_TORQUE_KEY_H
#define OPENTEC_BASE_BOARD_TORQUE_KEY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Events reported by the debounced Torque Key filter.
 *
 * An event is emitted only when the integrated input reaches a stable inserted or removed
 * endpoint.
 */
typedef enum {
    TORQUE_KEY_EVENT_NONE,     /**< No stable Torque Key transition occurred. */
    TORQUE_KEY_EVENT_INSERTED, /**< The Torque Key became stably inserted. */
    TORQUE_KEY_EVENT_REMOVED,  /**< The Torque Key became stably removed. */
} TorqueKeyEvent;

/**
 * @brief Stateful Torque Key debounce filter.
 *
 * The filter integrates elapsed time between the removed and inserted endpoints and separately
 * records whether a stable state has been reported.
 */
typedef struct {
    uint32_t last_update_ms;     /**< Timestamp of the preceding update in milliseconds. */
    uint16_t filter_position_ms; /**< Integrator position from inserted zero to removed 500 ms. */
    bool inserted;               /**< Last reported stable state; true when the key is inserted. */
    bool initialized;            /**< True after the first update establishes the time origin. */
    bool state_known;            /**< True after an inserted or removed event has been reported. */
} TorqueKey;

/**
 * @brief Initializes Torque Key transition filtering.
 *
 * Starts the integrator at its neutral midpoint without reporting a stable key state; the first
 * update establishes the time origin.
 *
 * @param[out] key Torque Key filter to initialize.
 */
void torque_key_init(TorqueKey *key);

/**
 * @brief Filters a raw Torque Key input into stable transitions.
 *
 * Integrates elapsed milliseconds toward the inserted or removed endpoint, clamps each interval
 * to the filter duration, and emits at most one transition per update.
 *
 * @param[in,out] key Torque Key filter state.
 * @param[in] raw_inserted True when the logical Torque Key input reports insertion.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Stable insertion or removal transition, or TORQUE_KEY_EVENT_NONE.
 */
TorqueKeyEvent torque_key_update(TorqueKey *key, bool raw_inserted, uint32_t now_ms);

#endif
