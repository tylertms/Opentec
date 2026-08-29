#ifndef OPENTEC_BASE_WHEEL_POSITION_H
#define OPENTEC_BASE_WHEEL_POSITION_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_POSITION_COUNTS_PER_REVOLUTION = 23680,
    WHEEL_POSITION_SAMPLE_LIMIT = 82880,
};

typedef struct {
    int32_t center;
    bool calibrated;
} WheelPositionReference;

typedef struct {
    int32_t center;
    uint32_t travel;
    uint32_t deadband;
} WheelPositionCalibration;

typedef struct {
    float filtered_position;
    float filtered_velocity;
    uint32_t next_update_ms;
    int32_t scaled_velocity;
} WheelVelocityEstimator;

int32_t wheel_position_center(int32_t sample, int32_t center);
int32_t wheel_position_filter(int32_t sample, const WheelPositionCalibration *calibration);
int16_t wheel_position_axis(int32_t sample, const WheelPositionCalibration *calibration);
uint16_t wheel_position_hid_axis(int32_t sample, const WheelPositionCalibration *calibration);
int16_t wheel_position_display_rotation(int32_t sample,
                                        const WheelPositionCalibration *calibration);
void wheel_position_reference_reset(WheelPositionReference *reference);
bool wheel_position_reference_capture(WheelPositionReference *reference, int32_t sample,
                                      uint32_t modulus);
uint32_t wheel_position_travel_from_degrees(uint16_t rotation_degrees);
WheelPositionCalibration wheel_position_calibration_build(const WheelPositionReference *reference,
                                                          uint16_t rotation_degrees,
                                                          uint8_t deadzone);
void wheel_velocity_reset(WheelVelocityEstimator *estimator);
int32_t wheel_velocity_update(WheelVelocityEstimator *estimator, int32_t position,
                              uint32_t time_ms);

#endif
