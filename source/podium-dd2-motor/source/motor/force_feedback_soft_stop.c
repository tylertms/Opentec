#include "motor/force_feedback_soft_stop.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    FORCE_LIMIT = 65535,
    RAMP_PERIOD_TICKS = 50,
    RAMP_MAXIMUM_PERCENT = 100,
    RANGE_REDUCTION_RESET_THRESHOLD = 480,
};

/**
 * @brief Applies the official travel-limit force ramp and selects the internal damper state.
 *
 * Penetration beyond either centered travel limit blends the current force toward the restoring
 * endpoint while range reductions recover through the fifty-tick ramp.
 *
 * @param soft_stop Travel-limit ramp state.
 * @param now Current motor service tick.
 * @param half_range Configured positive travel limit relative to center.
 * @param center Configured encoder center.
 * @param position Current encoder position.
 * @param transition_range Encoder distance over which force reaches full scale.
 * @param disabled True when the motor status suppresses travel-limit effects.
 * @param force Primary force command to update.
 * @return True when the internal travel-limit damper must be active.
 */
bool motor_force_feedback_soft_stop_apply(MotorForceFeedbackSoftStop *soft_stop, uint32_t now,
                                          int32_t half_range, int32_t center, int32_t position,
                                          uint16_t transition_range, bool disabled,
                                          int32_t *force) {
    if (soft_stop->previous_half_range - half_range > RANGE_REDUCTION_RESET_THRESHOLD) {
        soft_stop->ramp_percent = 0U;
    }
    soft_stop->previous_half_range = half_range;

    if (soft_stop->ramp_percent < RAMP_MAXIMUM_PERCENT && soft_stop->next_ramp_tick < now) {
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
