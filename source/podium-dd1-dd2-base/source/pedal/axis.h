#ifndef OPENTEC_BASE_PEDAL_AXIS_H
#define OPENTEC_BASE_PEDAL_AXIS_H

#include <stdint.h>

typedef struct {
    uint16_t minimum;
    uint16_t maximum;
    uint16_t lower_deadzone;
    uint16_t upper_deadzone;
    uint16_t output_scale;
    uint8_t learn_minimum;
    uint8_t learn_maximum;
} PedalAxisCalibration;

uint16_t pedal_axis_update(PedalAxisCalibration *calibration, uint16_t sample);

#endif
