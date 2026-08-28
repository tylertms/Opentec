#include <assert.h>
#include <stdint.h>

#include "analog/samples.h"

int main(void) {
    uint16_t scan[ANALOG_SCAN_SAMPLE_COUNT] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    AnalogSamples samples;
    analog_samples_decode(scan, &samples);

    assert(samples.primary_thermistor == 10);
    assert(samples.secondary_thermistor == 20);
    assert(samples.auxiliary_axis == 30);
    assert(samples.primary_shifter_y == 40);
    assert(samples.primary_shifter_x == 50);
    assert(samples.secondary_shifter_y == 60);
    assert(samples.secondary_shifter_x == 70);
    assert(samples.pedal_axes[0] == 80);
    assert(samples.pedal_axes[1] == 90);
    assert(samples.pedal_axes[2] == 100);
    return 0;
}
