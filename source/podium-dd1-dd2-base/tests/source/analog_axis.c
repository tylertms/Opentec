#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include "analog/axis.h"

static void test_unipolar(void) {
    AnalogUnipolarCalibration calibration = {
        .minimum = 100,
        .maximum = 1100,
        .inverted = 0,
    };

    assert(analog_axis_unipolar(0, &calibration) == 0);
    assert(analog_axis_unipolar(100, &calibration) == 0);
    assert(analog_axis_unipolar(600, &calibration) == 32767);
    assert(analog_axis_unipolar(1100, &calibration) == UINT16_MAX);
    assert(analog_axis_unipolar(UINT16_MAX, &calibration) == UINT16_MAX);

    calibration.inverted = 1;
    assert(analog_axis_unipolar(100, &calibration) == UINT16_MAX);
    assert(analog_axis_unipolar(1100, &calibration) == 0);
}

static void test_bipolar(void) {
    AnalogBipolarCalibration calibration = {
        .minimum = 100,
        .center = 600,
        .maximum = 1600,
        .inverted = 0,
    };

    assert(analog_axis_bipolar(0, &calibration) == INT16_MIN);
    assert(analog_axis_bipolar(100, &calibration) == INT16_MIN);
    assert(analog_axis_bipolar(350, &calibration) == -16384);
    assert(analog_axis_bipolar(600, &calibration) == 0);
    assert(analog_axis_bipolar(1100, &calibration) == 16383);
    assert(analog_axis_bipolar(1600, &calibration) == INT16_MAX);
    assert(analog_axis_bipolar(UINT16_MAX, &calibration) == INT16_MAX);

    calibration.inverted = 1;
    assert(analog_axis_bipolar(100, &calibration) == INT16_MAX);
    assert(analog_axis_bipolar(1600, &calibration) == -INT16_MAX);
}

static void test_invalid_calibration(void) {
    const AnalogUnipolarCalibration unipolar = {0};
    const AnalogBipolarCalibration bipolar = {0};

    assert(analog_axis_unipolar(UINT16_MAX, &unipolar) == 0);
    assert(analog_axis_bipolar(UINT16_MAX, &bipolar) == 0);
}

int main(void) {
    test_unipolar();
    test_bipolar();
    test_invalid_calibration();
    return 0;
}
