#include "force_feedback/soft_stop.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Travel-limit force and ramp configuration constants. */
enum {
    FORCE_LIMIT = 65535,                    /**< Maximum absolute travel-limit force. */
    RAMP_PERIOD_TICKS = 50,                 /**< Service ticks between ramp increments. */
    RAMP_MAXIMUM_PERCENT = 100,             /**< Full travel-limit ramp percentage. */
    RANGE_REDUCTION_RESET_THRESHOLD = 480,  /**< Range reduction that restarts the ramp. */
};

/**
 * @brief Tests whether a wrap-safe soft-stop ramp deadline has passed.
 *
 * Signed tick subtraction preserves ordering across one unsigned counter wrap.
 *
 * @param[in] now Current motor service tick.
 * @param[in] deadline Scheduled ramp tick.
 * @return True only after the scheduled tick has passed.
 */
static bool motor_force_feedback_tick_passed(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) > 0;
}

bool motor_force_feedback_soft_stop_apply(MotorForceFeedbackSoftStop *soft_stop, uint32_t now,
                                          int32_t half_range, int32_t center, int32_t position,
                                          uint16_t transition_range, bool disabled,
                                          int32_t *force) {
    if (soft_stop->previous_half_range - half_range > RANGE_REDUCTION_RESET_THRESHOLD) {
        soft_stop->ramp_percent = 0U;
    }
    soft_stop->previous_half_range = half_range;

    if (soft_stop->ramp_percent < RAMP_MAXIMUM_PERCENT &&
        motor_force_feedback_tick_passed(now, soft_stop->next_ramp_tick)) {
        ++soft_stop->ramp_percent;
        soft_stop->next_ramp_tick = now + RAMP_PERIOD_TICKS;
    }

    int32_t target = 0;
    uint32_t penetration = 0U;
    int32_t centered_position = position - center;
    if (centered_position > half_range) {
        target = -FORCE_LIMIT;
        penetration = (uint32_t)(centered_position - half_range);
    } else if (centered_position < -half_range) {
        target = FORCE_LIMIT;
        penetration = (uint32_t)(-centered_position - half_range);
    }

    if (penetration > transition_range) {
        penetration = transition_range;
    }
    if (disabled) {
        return false;
    }

    int32_t correction = (target - *force) * (int32_t)penetration / transition_range;
    correction = correction * soft_stop->ramp_percent / RAMP_MAXIMUM_PERCENT;
    *force += correction;
    return penetration != 0U;
}
