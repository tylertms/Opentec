#ifndef OPENTEC_BASE_ANALOG_SAMPLES_H
#define OPENTEC_BASE_ANALOG_SAMPLES_H

#include <stdint.h>

enum { ANALOG_SCAN_SAMPLE_COUNT = 10 };

typedef struct {
    uint16_t resistance_left;
    uint16_t resistance_right;
    uint16_t auxiliary_axis;
    uint16_t primary_shifter_y;
    uint16_t primary_shifter_x;
    uint16_t secondary_shifter_y;
    uint16_t secondary_shifter_x;
    uint16_t pedal_axes[3];
} AnalogSamples;

void analog_samples_decode(const volatile uint16_t scan[ANALOG_SCAN_SAMPLE_COUNT],
                           AnalogSamples *samples);

#endif
