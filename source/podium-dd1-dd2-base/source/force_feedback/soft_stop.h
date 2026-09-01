#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SOFT_STOP_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SOFT_STOP_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Configuration for the wheel-range soft stop.
 *
 * The travel limit is the one-sided centered-position boundary used to report outside-travel
 * status and to place the force-ramp onset beyond either end of the wheel range.
 */
typedef struct {
    int32_t travel_limit; /**< One-sided centered-position travel limit. */
} ForceSoftStopConfig;

/**
 * @brief Runtime state for the wheel-range soft-stop ramp.
 *
 * The state remembers the prior force-ramp boundary and schedules the percentage increase applied
 * to end-stop force additions.
 */
typedef struct {
    int32_t previous_boundary; /**< Previous travel limit plus the onset margin. */
    uint32_t next_ramp_ms;     /**< Deadline after which the ramp percentage may increase. */
    uint8_t ramp_percent;      /**< Current end-stop force-ramp percentage. */
} ForceSoftStopState;

/**
 * @brief Result of applying the wheel-range soft stop.
 *
 * The force includes any end-stop addition, while the status flag reports the configured travel
 * boundary independently of the soft-stop onset margin.
 */
typedef struct {
    int32_t force;       /**< Accumulated force after the soft-stop addition. */
    bool outside_travel; /**< Whether the position lies outside the configured travel limit. */
} ForceSoftStopResult;

/**
 * @brief Resets wheel-range soft-stop runtime state.
 *
 * Clears the previous boundary, ramp deadline, and ramp percentage so the next update starts with
 * a fresh end-stop ramp.
 *
 * @param[out] state Soft-stop runtime state to clear.
 */
void force_soft_stop_reset(ForceSoftStopState *state);

/**
 * @brief Applies the wheel-range soft stop to an accumulated force.
 *
 * Begins the end-stop force addition 1000 position counts beyond each configured boundary. When
 * the current time is later than the next 50-millisecond deadline, it raises the ramp by one
 * percent. An inward boundary movement of at least 494 counts restarts the ramp. A disabled
 * secondary output leaves the force unchanged and reports the position as inside travel.
 *
 * @param[in,out] state Soft-stop ramp state to update.
 * @param[in] config Current one-sided wheel travel limit.
 * @param[in] position Centered wheel position.
 * @param[in] accumulated_force Force accumulated before the soft stop is applied.
 * @param[in] output_disabled Whether the secondary output is disabled.
 * @param[in] now_ms Current system time in milliseconds.
 * @return Force after the soft-stop addition and the configured-boundary status.
 */
ForceSoftStopResult force_soft_stop_update(ForceSoftStopState *state,
                                           const ForceSoftStopConfig *config, int32_t position,
                                           int32_t accumulated_force, bool output_disabled,
                                           uint32_t now_ms);

#endif
