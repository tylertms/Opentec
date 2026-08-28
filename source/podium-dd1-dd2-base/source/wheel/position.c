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

void wheel_velocity_reset(WheelVelocityEstimator *estimator) {
    memset(estimator, 0, sizeof(*estimator));
}

int32_t wheel_velocity_update(WheelVelocityEstimator *estimator, int32_t position, uint32_t time_ms,
                              uint8_t response_percent) {
    position = clamp_position(position);
    if (estimator->initialized == 0) {
        estimator->previous_position = position;
        estimator->previous_time_ms = time_ms;
        estimator->initialized = 1;
        return 0;
    }

    uint32_t elapsed_ms = time_ms - estimator->previous_time_ms;
    if (elapsed_ms == 0) {
        return estimator->velocity;
    }
    if (response_percent > 100) {
        response_percent = 100;
    }

    int32_t distance = position - estimator->previous_position;
    int32_t measured_velocity = distance * 1000 / (int32_t)elapsed_ms;
    int32_t correction = measured_velocity - estimator->velocity;
    estimator->velocity += (int32_t)((int64_t)correction * response_percent / 100);
    estimator->previous_position = position;
    estimator->previous_time_ms = time_ms;
    return estimator->velocity;
}
