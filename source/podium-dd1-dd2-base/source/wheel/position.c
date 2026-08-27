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
