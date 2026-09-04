#ifndef OPENTEC_BASE_BOARD_TORQUE_KEY_H
#define OPENTEC_BASE_BOARD_TORQUE_KEY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Stateful Torque Key debounce filter.
 *
 * The filter integrates elapsed time between the removed and inserted endpoints and separately
 * records whether a stable state has been reported. Endpoint observations are handled before the
 * current raw sample, matching the firmware's reversal behavior.
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
 * Integrates elapsed milliseconds toward the inserted or removed endpoint and clamps each interval
 * to the filter duration. Endpoint state is retained for consumers that need the debounced level.
 *
 * @param[in,out] key Torque Key filter state.
 * @param[in] raw_inserted True when the logical Torque Key input reports insertion.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void torque_key_update(TorqueKey *key, bool raw_inserted, uint32_t now_ms);

#endif
