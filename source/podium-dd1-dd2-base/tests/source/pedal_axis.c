#include <assert.h>
#include <stdint.h>

#include "pedal/axis.h"

static void test_scales_with_deadzones(void) {
    PedalAxisCalibration calibration = {
        .minimum = 100,
        .maximum = 1100,
        .lower_deadzone = 100,
        .upper_deadzone = 100,
        .output_scale = UINT16_MAX,
    };

    assert(pedal_axis_update(&calibration, 100) == 0);
    assert(pedal_axis_update(&calibration, 200) == 0);
    assert(pedal_axis_update(&calibration, 600) == 32767);
    assert(pedal_axis_update(&calibration, 1000) == UINT16_MAX);
    assert(pedal_axis_update(&calibration, 1100) == UINT16_MAX);
}

static void test_learns_enabled_limits(void) {
    PedalAxisCalibration calibration = {
        .minimum = 200,
        .maximum = 800,
        .output_scale = UINT16_MAX,
        .learn_minimum = 1,
        .learn_maximum = 1,
    };

    assert(pedal_axis_update(&calibration, 100) == 0);
    assert(calibration.minimum == 100);
    assert(pedal_axis_update(&calibration, 1000) == UINT16_MAX);
    assert(calibration.maximum == 1000);
}

static void test_clamps_fixed_limits(void) {
    PedalAxisCalibration calibration = {
        .minimum = 200,
        .maximum = 800,
        .output_scale = UINT16_MAX,
    };

    assert(pedal_axis_update(&calibration, 100) == 0);
    assert(calibration.minimum == 200);
    assert(pedal_axis_update(&calibration, 900) == UINT16_MAX);
    assert(calibration.maximum == 800);
}

static void test_rejects_exhausted_range(void) {
    PedalAxisCalibration calibration = {
        .minimum = 100,
        .maximum = 200,
        .lower_deadzone = 60,
        .upper_deadzone = 60,
        .output_scale = UINT16_MAX,
    };

    assert(pedal_axis_update(&calibration, 150) == 0);
}

static void test_requires_margin_width_beyond_the_scaled_span(void) {
    PedalAxisCalibration calibration = {
        .minimum = 100,
        .maximum = 500,
        .lower_deadzone = 150,
        .upper_deadzone = 100,
        .output_scale = UINT16_MAX,
    };

    assert(pedal_axis_update(&calibration, 350) == 0);
}

static void test_uses_upper_margin_when_maximum_is_below_it(void) {
    PedalAxisCalibration calibration = {
        .minimum = 0,
        .maximum = 50,
        .upper_deadzone = 100,
        .output_scale = UINT16_MAX,
    };

    assert(pedal_axis_update(&calibration, 50) == 32767);
}

static void test_limits_output_to_configured_scale(void) {
    PedalAxisCalibration calibration = {
        .minimum = 0,
        .maximum = 1000,
        .output_scale = 255,
    };

    assert(pedal_axis_update(&calibration, 500) == 127);
    assert(pedal_axis_update(&calibration, 1000) == 255);
}

int main(void) {
    test_scales_with_deadzones();
    test_learns_enabled_limits();
    test_clamps_fixed_limits();
    test_rejects_exhausted_range();
    test_requires_margin_width_beyond_the_scaled_span();
    test_uses_upper_margin_when_maximum_is_below_it();
    test_limits_output_to_configured_scale();
    return 0;
}
