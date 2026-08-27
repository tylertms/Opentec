#ifndef OPENTEC_BASE_ANALOG_AXIS_H
#define OPENTEC_BASE_ANALOG_AXIS_H

#include <stdint.h>

enum {
    ANALOG_AXIS_FILTER_SAMPLES = 5,
};

typedef struct {
    uint32_t total;
    uint16_t samples[ANALOG_AXIS_FILTER_SAMPLES];
    uint16_t value;
    uint8_t count;
    uint8_t next_sample;
} AnalogAxisFilter;

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
uint16_t analog_axis_filter(AnalogAxisFilter *filter, uint16_t sample, uint16_t deadband);

#endif
