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

int32_t wheel_position_center(int32_t sample, int32_t center);
int32_t wheel_position_filter(int32_t sample, const WheelPositionCalibration *calibration);
int16_t wheel_position_axis(int32_t sample, const WheelPositionCalibration *calibration);
uint16_t wheel_position_hid_axis(int32_t sample, const WheelPositionCalibration *calibration);

#endif
