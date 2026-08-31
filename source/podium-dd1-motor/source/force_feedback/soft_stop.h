#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_SOFT_STOP_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_SOFT_STOP_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief State for the motor travel-limit force ramp.
 *
 * Retains the previous steering range and the wrap-safe deadline used to restore the soft-stop
 * force after a range change.
 */
typedef struct {
    int32_t previous_half_range; /**< Previous configured positive travel half-range. */
    uint32_t next_ramp_tick;     /**< Next motor service tick for ramp advancement. */
    uint8_t ramp_percent;        /**< Current travel-limit force ramp percentage. */
} MotorForceFeedbackSoftStop;

/**
 * @brief Applies the travel-limit force correction and reports damper activation.
 *
 * Penetration beyond either centered travel limit blends the current force toward the restoring
 * endpoint. A large range reduction restarts the fifty-tick recovery ramp.
 *
 * @param[in,out] soft_stop Travel-limit ramp state.
 * @param[in] now Current motor service tick.
 * @param[in] half_range Configured positive travel limit relative to center.
 * @param[in] center Configured encoder center.
 * @param[in] position Current extended encoder position.
 * @param[in] transition_range Encoder distance over which force reaches full scale.
 * @param[in] disabled True when motor status suppresses travel-limit effects.
 * @param[in,out] force Primary force command to update.
 * @return True when travel-limit penetration requires the internal damper effect.
 */
bool motor_force_feedback_soft_stop_apply(MotorForceFeedbackSoftStop *soft_stop, uint32_t now,
                                          int32_t half_range, int32_t center, int32_t position,
                                          uint16_t transition_range, bool disabled, int32_t *force);

#endif
