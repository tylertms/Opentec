#include "wheel/position.h"

#include <stdint.h>
#include <string.h>

/**
 * @brief Clamps a signed wheel position to the supported sensor range.
 *
 * Saturates values outside the one-sided 82,880-count limit and preserves values within it.
 *
 * @param[in] position Signed wheel position to constrain.
 * @return Position constrained to the supported sensor range.
 */
static int32_t clamp_position(int32_t position) {
    if (position < -(int32_t)WHEEL_POSITION_SAMPLE_LIMIT) {
        return -(int32_t)WHEEL_POSITION_SAMPLE_LIMIT;
    }
    if (position > (int32_t)WHEEL_POSITION_SAMPLE_LIMIT) {
        return (int32_t)WHEEL_POSITION_SAMPLE_LIMIT;
    }
    return (int32_t)position;
}

/**
 * @brief Detects either filter sentinel bit pattern.
 *
 * Interprets the floating-point value as its 32-bit representation and matches the two values the
 * position filter treats as invalid.
 *
 * @param[in] value Floating-point filter value.
 * @return True when the value has either invalid sentinel representation.
 */
static bool filter_value_is_invalid(float value) {
    /** @brief Bit-preserving view of a filter value. */
    union {
        float value;   /**< Floating-point value under inspection. */
        uint32_t bits; /**< Raw IEEE-754 representation. */
    } representation = {.value = value};
    return representation.bits == UINT32_C(0x7fffffff) || representation.bits == UINT32_MAX;
}

/**
 * @brief Centers an absolute wheel-position sample.
 *
 * Subtracts the retained center reference and saturates the result to the supported sensor range.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] center Retained absolute center reference.
 * @return Signed and constrained displacement from center.
 */
int32_t wheel_position_center(int32_t sample, int32_t center) {
    if (center > 0 && sample < INT32_MIN + center) {
        return -(int32_t)WHEEL_POSITION_SAMPLE_LIMIT;
    }
    if (center < 0 && sample > INT32_MAX + center) {
        return (int32_t)WHEEL_POSITION_SAMPLE_LIMIT;
    }
    return clamp_position(sample - center);
}

/**
 * @brief Applies wheel centering and steering deadband.
 *
 * Removes the configured deadband from the magnitude outside the center region and returns zero
 * while the centered position remains within that region.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Active wheel center, travel, and deadband.
 * @return Centered position with the deadband removed.
 */
int32_t wheel_position_filter(int32_t sample, const WheelPositionCalibration *calibration) {
    int32_t position = wheel_position_center(sample, calibration->center);
    uint32_t deadband = calibration->deadband;

    if (deadband > WHEEL_POSITION_SAMPLE_LIMIT) {
        deadband = WHEEL_POSITION_SAMPLE_LIMIT;
    }
    if (position > -(int32_t)deadband && position < (int32_t)deadband) {
        return 0;
    }
    return position < 0 ? position + (int32_t)deadband : position - (int32_t)deadband;
}

/**
 * @brief Scales a wheel-position sample to a signed HID axis.
 *
 * Applies centering and deadband, scales negative travel to 32,768 steps and positive travel to
 * 32,767 steps, and saturates samples at the configured end stops.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Active wheel center, travel, and deadband.
 * @return Signed sixteen-bit steering axis.
 */
int16_t wheel_position_axis(int32_t sample, const WheelPositionCalibration *calibration) {
    int32_t position = wheel_position_filter(sample, calibration);
    uint32_t travel = calibration->travel;

    if (travel == 0) {
        return 0;
    }
    if (travel > WHEEL_POSITION_SAMPLE_LIMIT) {
        travel = WHEEL_POSITION_SAMPLE_LIMIT;
    }

    float limit = position < 0 ? 32768.0f : 32767.0f;
    float scaled = limit / (float)travel * (float)position;
    if (scaled > 32767.0f) {
        scaled = 32767.0f;
    } else if (scaled < -32768.0f) {
        scaled = -32768.0f;
    }
    return (int16_t)(int32_t)scaled;
}

/**
 * @brief Encodes a wheel-position sample as an unsigned HID steering axis.
 *
 * Offsets the signed steering axis by 32,768 so full left maps to zero and full right maps to
 * 65,535.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Active wheel center, travel, and deadband.
 * @return Unsigned sixteen-bit steering axis.
 */
uint16_t wheel_position_hid_axis(int32_t sample, const WheelPositionCalibration *calibration) {
    return (uint16_t)((int32_t)wheel_position_axis(sample, calibration) + 32768);
}

/**
 * @brief Calculates the attached-wheel display rotation angle.
 *
 * Converts the centered, deadband-adjusted position to hundredths of a degree, clamps it to the
 * configured steering travel, and folds successive half turns into the signed range from -18000
 * through 18000.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Active wheel center, travel, and deadband.
 * @return Signed display angle in hundredths of a degree.
 */
int16_t wheel_position_display_rotation(int32_t sample,
                                        const WheelPositionCalibration *calibration) {
    int16_t axis = wheel_position_axis(sample, calibration);
    uint32_t travel = calibration->travel > WHEEL_POSITION_SAMPLE_LIMIT
                          ? WHEEL_POSITION_SAMPLE_LIMIT
                          : calibration->travel;
    float angle = (float)((int32_t)travel * 100);
    angle /= 32.888889f;
    angle = (float)axis * angle;
    angle /= 65535.0f;

    int32_t scaled = (int32_t)angle;
    int16_t quotient = (int16_t)(scaled / 18000);
    if (scaled > 18000) {
        return (int16_t)((quotient & 1) != 0 ? scaled % 18000 - 18000 : scaled % 18000);
    }
    if (scaled < -18000) {
        return (int16_t)((quotient & 1) != 0 ? scaled % 18000 + 18000 : scaled % 18000);
    }
    return (int16_t)scaled;
}

/**
 * @brief Clears the retained wheel center reference.
 *
 * Resets the absolute center to zero and marks the reference unavailable.
 *
 * @param[out] reference Wheel center reference to clear.
 */
void wheel_position_reference_reset(WheelPositionReference *reference) {
    reference->center = 0;
    reference->calibrated = false;
}

/**
 * @brief Captures an absolute wheel sample as the center reference.
 *
 * Reduces the sample with the motor controller's signed position modulus, stores the normalized
 * value, marks the reference available, and reports whether persistence-visible state changed.
 *
 * @param[in,out] reference Wheel center reference to update.
 * @param[in] sample Absolute wheel-position sample to retain.
 * @param[in] modulus Motor-controller position modulus.
 * @return True when the stored center or availability state changed.
 */
bool wheel_position_reference_capture(WheelPositionReference *reference, int32_t sample,
                                      uint32_t modulus) {
    int32_t center = sample % (int32_t)modulus;
    bool changed = !reference->calibrated || reference->center != center;
    reference->center = center;
    reference->calibrated = true;
    return changed;
}

/**
 * @brief Convert a configured wheel range to a one-sided position limit.
 *
 * The device represents one full revolution with 23680 position counts and limits the configured
 * range to 2520 degrees. The returned limit is the distance from center to either end stop.
 *
 * @param[in] rotation_degrees Configured lock-to-lock wheel range in degrees.
 * @return One-sided position limit in wheel counts, capped at the 2520-degree device limit.
 */
uint32_t wheel_position_travel_from_degrees(uint16_t rotation_degrees) {
    uint32_t travel = (uint32_t)rotation_degrees * WHEEL_POSITION_COUNTS_PER_REVOLUTION / 720;
    return travel > WHEEL_POSITION_SAMPLE_LIMIT ? WHEEL_POSITION_SAMPLE_LIMIT : travel;
}

/**
 * @brief Builds the active wheel-position calibration.
 *
 * Combines the retained center, configured lock-to-lock range, and ten-count deadzone steps. Travel
 * remains disabled until a center reference is available.
 *
 * @param[in] reference Retained wheel center reference.
 * @param[in] rotation_degrees Configured lock-to-lock wheel range in degrees.
 * @param[in] deadzone Configured steering deadzone in ten-count steps.
 * @return Complete wheel-position calibration for input and display processing.
 */
WheelPositionCalibration wheel_position_calibration_build(const WheelPositionReference *reference,
                                                          uint16_t rotation_degrees,
                                                          uint8_t deadzone) {
    return (WheelPositionCalibration){
        .center = reference->center,
        .travel = reference->calibrated ? wheel_position_travel_from_degrees(rotation_degrees) : 0,
        .deadband = (uint32_t)deadzone * 10,
    };
}

/**
 * @brief Clears the wheel-velocity filter state.
 *
 * Resets the filtered position, filtered velocity, update deadline, and last scaled output.
 *
 * @param[out] estimator Velocity filter state to initialize.
 */
void wheel_velocity_reset(WheelVelocityEstimator *estimator) {
    memset(estimator, 0, sizeof(*estimator));
}

/**
 * @brief Updates the filtered wheel velocity at one-millisecond intervals.
 *
 * Predicts position from the previous filter state, corrects position by half the residual, and
 * corrects velocity by sixty times the residual. The signed result uses the device scale of 132
 * filter units per reported velocity unit.
 *
 * @param[in,out] estimator Position and velocity filter state.
 * @param[in] position Current centered wheel position.
 * @param[in] time_ms Current monotonic time in milliseconds.
 * @return Latest signed and scaled wheel velocity.
 */
int32_t wheel_velocity_update(WheelVelocityEstimator *estimator, int32_t position,
                              uint32_t time_ms) {
    if (time_ms < estimator->next_update_ms) {
        return estimator->scaled_velocity;
    }

    estimator->next_update_ms = time_ms + 1;
    float predicted_position = estimator->filtered_position + estimator->filtered_velocity * 0.001f;
    float residual = (float)position - predicted_position;
    if (filter_value_is_invalid(residual)) {
        residual = 0.0f;
    }

    estimator->filtered_position = predicted_position + residual * 0.5f;
    if (filter_value_is_invalid(estimator->filtered_position)) {
        estimator->filtered_position = (float)position;
    }

    estimator->filtered_velocity += residual * 0.06f / 0.001f;
    if (filter_value_is_invalid(estimator->filtered_velocity)) {
        estimator->filtered_velocity = 0.0f;
    }

    estimator->scaled_velocity = -(int32_t)estimator->filtered_velocity / 132;
    return estimator->scaled_velocity;
}
