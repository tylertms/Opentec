#ifndef OPENTEC_BASE_WHEEL_POSITION_H
#define OPENTEC_BASE_WHEEL_POSITION_H

#include <stdint.h>

enum {
    WHEEL_POSITION_SAMPLE_LIMIT = 82880,
};

typedef struct {
    int32_t center;
    uint32_t travel;
    uint32_t deadband;
} WheelPositionCalibration;

typedef struct {
    int32_t previous_position;
    int32_t velocity;
    uint32_t previous_time_ms;
    uint8_t initialized;
} WheelVelocityEstimator;

int32_t wheel_position_center(int32_t sample, int32_t center);
int32_t wheel_position_filter(int32_t sample, const WheelPositionCalibration *calibration);
int16_t wheel_position_axis(int32_t sample, const WheelPositionCalibration *calibration);
uint16_t wheel_position_hid_axis(int32_t sample, const WheelPositionCalibration *calibration);
void wheel_velocity_reset(WheelVelocityEstimator *estimator);
int32_t wheel_velocity_update(WheelVelocityEstimator *estimator, int32_t position, uint32_t time_ms,
                              uint8_t response_percent);

#endif
