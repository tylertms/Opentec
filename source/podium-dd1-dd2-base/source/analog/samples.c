#include "analog/samples.h"

void analog_samples_decode(const volatile uint16_t scan[ANALOG_SCAN_SAMPLE_COUNT],
                           AnalogSamples *samples) {
    samples->resistance_left = scan[0];
    samples->resistance_right = scan[1];
    samples->auxiliary_axis = scan[2];
    samples->primary_shifter_y = scan[3];
    samples->primary_shifter_x = scan[4];
    samples->secondary_shifter_y = scan[5];
    samples->secondary_shifter_x = scan[6];
}
