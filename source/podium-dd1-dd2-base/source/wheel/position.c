#include "wheel/position.h"

#include <stdint.h>
#include <string.h>

static int32_t clamp_position(int64_t position) {
    if (position < -WHEEL_POSITION_SAMPLE_LIMIT) {
        return -WHEEL_POSITION_SAMPLE_LIMIT;
    }
    if (position > WHEEL_POSITION_SAMPLE_LIMIT) {
        return WHEEL_POSITION_SAMPLE_LIMIT;
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
    union {
        float value;
        uint32_t bits;
    } representation = {.value = value};
    return representation.bits == UINT32_C(0x7fffffff) || representation.bits == UINT32_MAX;
}

int32_t wheel_position_center(int32_t sample, int32_t center) {
    return clamp_position((int64_t)sample - center);
}

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

int16_t wheel_position_axis(int32_t sample, const WheelPositionCalibration *calibration) {
    int32_t position = wheel_position_filter(sample, calibration);
    uint32_t travel = calibration->travel;

    if (travel == 0) {
        return 0;
    }
    if (travel > WHEEL_POSITION_SAMPLE_LIMIT) {
        travel = WHEEL_POSITION_SAMPLE_LIMIT;
    }

    uint32_t magnitude = position < 0 ? (uint32_t)-position : (uint32_t)position;
    uint32_t axis_limit = position < 0 ? 32768u : 32767u;
    uint32_t scaled = magnitude >= travel ? axis_limit : magnitude * axis_limit / travel;
    return position < 0 ? (int16_t)-(int32_t)scaled : (int16_t)scaled;
}

uint16_t wheel_position_hid_axis(int32_t sample, const WheelPositionCalibration *calibration) {
    return (uint16_t)((int32_t)wheel_position_axis(sample, calibration) + 32768);
}

void wheel_position_reference_reset(WheelPositionReference *reference) {
    reference->center = 0;
    reference->calibrated = false;
}

bool wheel_position_reference_capture(WheelPositionReference *reference, int32_t sample) {
    bool changed = !reference->calibrated || reference->center != sample;
    reference->center = sample;
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
