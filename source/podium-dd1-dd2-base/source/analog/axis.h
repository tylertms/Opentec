#ifndef OPENTEC_BASE_ANALOG_AXIS_H
#define OPENTEC_BASE_ANALOG_AXIS_H

#include <stdint.h>

typedef struct {
    uint16_t minimum;
    uint16_t maximum;
    uint8_t inverted;
} AnalogUnipolarCalibration;

typedef struct {
    uint16_t minimum;
    uint16_t center;
    uint16_t maximum;
    uint8_t inverted;
} AnalogBipolarCalibration;

uint16_t analog_axis_unipolar(uint16_t sample, const AnalogUnipolarCalibration *calibration);
int16_t analog_axis_bipolar(uint16_t sample, const AnalogBipolarCalibration *calibration);

#endif
