#include "motor/motion.h"

#include <limits.h>

/**
 * @brief Limits one signed value to the sixteen-bit motor range.
 *
 * Values inside the range pass through unchanged.
 *
 * @param[in] value Signed value to limit.
 * @return Sixteen-bit saturated result.
 */
static int16_t motor_int16_saturate(int32_t value) {
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

/**
 * @brief Limits one wide accumulator to the signed thirty-two-bit range.
 *
 * Values inside the range pass through unchanged.
 *
 * @param[in] value Wide signed accumulator value.
 * @return Thirty-two-bit saturated result.
 */
static int32_t motor_int32_saturate(int64_t value) {
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

/**
 * @brief Applies the official signed Q15 motor scale with sixteen-bit saturation.
 *
 * The recovered mixed-width multiply is limited to the signed sixteen-bit output range.
 *
 * @param[in] scale Unsigned fixed-point scale.
 * @param[in] value Signed input value.
 * @return Scaled signed value limited to the sixteen-bit range.
 */
int16_t motor_q15_scale_saturate(uint32_t scale, int16_t value) {
    int32_t upper = (int32_t)scale >> 16;
    int32_t lower = (int32_t)(scale & UINT16_MAX);
    int32_t result = upper * value * 2 + (lower * value >> 15);
    return motor_int16_saturate(result);
}

/**
 * @brief Applies the official signed Q15 motor scale with sixteen-bit wrapping.
 *
 * The recovered mixed-width multiply retains the low signed sixteen-bit result.
 *
 * @param[in] scale Unsigned fixed-point scale.
 * @param[in] value Signed input value.
 * @return Low sixteen bits of the scaled signed value.
 */
int16_t motor_q15_scale_wrap(uint32_t scale, int16_t value) {
    int64_t upper = (int16_t)(scale >> 16U);
    int64_t lower = scale & UINT16_MAX;
    int64_t result = upper * value * 2 + (lower * value >> 15U);
    return (int16_t)(uint16_t)result;
}

/**
 * @brief Calculates the official saturated difference between two signed samples.
 *
 * Subtraction clamps at the signed sixteen-bit endpoints.
 *
 * @param[in] value Current sample.
 * @param[in] previous Previous sample.
 * @return Signed difference limited to the sixteen-bit range.
 */
int16_t motor_signed_difference_saturate(int16_t value, int16_t previous) {
    return motor_int16_saturate((int32_t)value - previous);
}

/**
 * @brief Advances the official saturating leaky-accumulator filter.
 *
 * The current input is accumulated, the shifted output is published, and that output leaks back
 * out of the persistent accumulator.
 *
 * @param[in,out] filter Persistent filter accumulator and binary shift.
 * @param[in] sample Current signed input sample.
 * @return Filter output limited to the sixteen-bit range.
 */
int16_t motor_motion_filter_step(MotorMotionFilter *filter, int16_t sample) {
    int32_t accumulated = motor_int32_saturate((int64_t)filter->accumulator + sample);
    int32_t output = accumulated >> (filter->shift & UINT8_MAX);
    filter->accumulator = motor_int32_saturate((int64_t)accumulated - output);
    return motor_int16_saturate(output);
}

/**
 * @brief Converts the wrapped hardware counter difference with the selected motor scale.
 *
 * Counter subtraction intentionally wraps before the board-specific fixed-point conversion.
 *
 * @param[in,out] state Persistent counter history.
 * @param[in] counter Current hardware counter value.
 * @param[in] scale Board-selected fixed-point scale.
 * @return Scaled signed counter difference.
 */
int16_t motor_encoder_delta_scale(MotorMotionState *state, uint32_t counter, uint32_t scale) {
    int16_t delta = (int16_t)((uint16_t)counter - (uint16_t)state->previous_counter);
    state->previous_counter = counter;
    return motor_q15_scale_saturate(scale, delta);
}

/**
 * @brief Converts the filtered position difference with the selected motor scale.
 *
 * Successive filtered positions produce the acceleration-like velocity delta channel.
 *
 * @param[in,out] state Persistent filtered-position history.
 * @param[in] filtered_delta Current filtered position delta.
 * @param[in] scale Board-selected fixed-point scale.
 * @return Scaled signed velocity difference.
 */
int16_t motor_velocity_delta_scale(MotorMotionState *state, int16_t filtered_delta,
                                   uint32_t scale) {
    int16_t delta =
        motor_signed_difference_saturate(filtered_delta, state->previous_filtered_delta);
    state->previous_filtered_delta = filtered_delta;
    return motor_q15_scale_saturate(scale, delta);
}

/**
 * @brief Reproduces the official four-stage motor position and velocity estimator.
 *
 * Wrapped count deltas feed the position filter, and successive filtered outputs feed the velocity
 * filter and its scaled derivative.
 *
 * @param[in,out] state Persistent estimator history.
 * @param[in,out] position_filter Position-delta filter configured with shift four.
 * @param[in,out] velocity_filter Velocity-delta filter configured with shift six.
 * @param[in] counter Current encoder counter value.
 * @param[in] scale Board-selected fixed-point scale.
 * @return Raw and filtered position and velocity deltas.
 */
MotorMotionSample motor_motion_sample(MotorMotionState *state, MotorMotionFilter *position_filter,
                                      MotorMotionFilter *velocity_filter, uint32_t counter,
                                      uint32_t scale) {
    MotorMotionSample sample;
    sample.position_delta = motor_encoder_delta_scale(state, counter, scale);
    sample.filtered_position_delta =
        motor_motion_filter_step(position_filter, sample.position_delta);
    sample.velocity_delta =
        motor_velocity_delta_scale(state, sample.filtered_position_delta, scale);
    sample.filtered_velocity_delta =
        motor_motion_filter_step(velocity_filter, sample.velocity_delta);
    return sample;
}
