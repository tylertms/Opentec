#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include "wheel/position.h"

static void test_centering(void) {
    assert(wheel_position_center(1000, 1000) == 0);
    assert(wheel_position_center(83880, 1000) == WHEEL_POSITION_SAMPLE_LIMIT);
    assert(wheel_position_center(-81880, 1000) == -WHEEL_POSITION_SAMPLE_LIMIT);
    assert(wheel_position_center(INT32_MAX, INT32_MIN) == WHEEL_POSITION_SAMPLE_LIMIT);
    assert(wheel_position_center(INT32_MIN, INT32_MAX) == -WHEEL_POSITION_SAMPLE_LIMIT);
}

static void test_deadband(void) {
    const WheelPositionCalibration calibration = {
        .center = 1000,
        .travel = WHEEL_POSITION_SAMPLE_LIMIT,
        .deadband = 100,
    };

    assert(wheel_position_filter(899, &calibration) == -1);
    assert(wheel_position_filter(900, &calibration) == 0);
    assert(wheel_position_filter(999, &calibration) == 0);
    assert(wheel_position_filter(1000, &calibration) == 0);
    assert(wheel_position_filter(1001, &calibration) == 0);
    assert(wheel_position_filter(1100, &calibration) == 0);
    assert(wheel_position_filter(1101, &calibration) == 1);
}

static void test_axis_scaling(void) {
    const WheelPositionCalibration calibration = {
        .center = 1000,
        .travel = WHEEL_POSITION_SAMPLE_LIMIT,
        .deadband = 0,
    };

    assert(wheel_position_axis(1000 - WHEEL_POSITION_SAMPLE_LIMIT, &calibration) == INT16_MIN);
    assert(wheel_position_axis(1000, &calibration) == 0);
    assert(wheel_position_axis(1000 + WHEEL_POSITION_SAMPLE_LIMIT, &calibration) == INT16_MAX);
    assert(wheel_position_hid_axis(1000 - WHEEL_POSITION_SAMPLE_LIMIT, &calibration) == 0);
    assert(wheel_position_hid_axis(1000, &calibration) == 32768);
    assert(wheel_position_hid_axis(1000 + WHEEL_POSITION_SAMPLE_LIMIT, &calibration) == UINT16_MAX);

    const WheelPositionCalibration fractional = {.center = 0, .travel = 10, .deadband = 0};
    assert(wheel_position_axis(-1, &fractional) == -3276);
    assert(wheel_position_hid_axis(-1, &fractional) == 29491);
}

static void test_invalid_travel(void) {
    const WheelPositionCalibration calibration = {
        .center = 0,
        .travel = 0,
        .deadband = 0,
    };

    assert(wheel_position_axis(INT32_MIN, &calibration) == 0);
    assert(wheel_position_axis(INT32_MAX, &calibration) == 0);
}

static void test_reference_capture(void) {
    WheelPositionReference reference;
    wheel_position_reference_reset(&reference);
    assert(!reference.calibrated);
    assert(wheel_position_reference_capture(&reference, 25000, UINT32_C(0x5c7f)));
    assert(reference.calibrated);
    assert(reference.center == 1321);
    assert(!wheel_position_reference_capture(&reference, 25000, UINT32_C(0x5c7f)));
    assert(wheel_position_reference_capture(&reference, -25000, UINT32_C(0x5c7f)));
    assert(reference.center == -1321);

    assert(wheel_position_reference_capture(&reference, 23680, UINT32_C(0x5c7f)));
    assert(reference.center == 1);
    assert(!wheel_position_reference_capture(&reference, 1, UINT32_C(0x5c7f)));
    assert(wheel_position_reference_capture(&reference, -23680, UINT32_C(0x5c7f)));
    assert(reference.center == -1);
}

static void test_calibration_building(void) {
    WheelPositionReference reference = {.center = 1234, .calibrated = true};
    WheelPositionCalibration calibration = wheel_position_calibration_build(&reference, 900, 4);
    assert(calibration.center == 1234);
    assert(calibration.travel == 29600);
    assert(calibration.deadband == 40);

    calibration = wheel_position_calibration_build(&reference, 2520, 0);
    assert(calibration.travel == WHEEL_POSITION_SAMPLE_LIMIT);

    reference.calibrated = false;
    calibration = wheel_position_calibration_build(&reference, 900, 0);
    assert(calibration.travel == 0);
    assert(wheel_position_hid_axis(INT32_MAX, &calibration) == 32768);
}

static void test_range_conversion(void) {
    assert(wheel_position_travel_from_degrees(0) == 0);
    assert(wheel_position_travel_from_degrees(900) == 29600);
    assert(wheel_position_travel_from_degrees(1080) == 35520);
    assert(wheel_position_travel_from_degrees(2520) == WHEEL_POSITION_SAMPLE_LIMIT);
    assert(wheel_position_travel_from_degrees(UINT16_MAX) == WHEEL_POSITION_SAMPLE_LIMIT);
}

static void test_display_rotation(void) {
    const WheelPositionCalibration calibration = {
        .center = 1000,
        .travel = 35520,
        .deadband = 0,
    };

    assert(wheel_position_hid_axis(1000, &calibration) == 32768);
    assert(wheel_position_display_rotation(35520) == 0);
    assert(wheel_position_hid_axis(1000 + 5920, &calibration) == 38229);
    assert(wheel_position_display_rotation(35520) == 8999);
    assert(wheel_position_hid_axis(1000 + 35520, &calibration) == UINT16_MAX);
    assert(wheel_position_display_rotation(35520) == 17999);
    assert(wheel_position_hid_axis(1000 - 35520, &calibration) == 0);
    assert(wheel_position_display_rotation(35520) == 18000);

    WheelPositionCalibration deadband = calibration;
    deadband.deadband = 10;
    assert(wheel_position_hid_axis(1010, &deadband) == 32768);
    assert(wheel_position_display_rotation(35520) == 0);

    assert(wheel_position_hid_axis(1000 + 5920, &calibration) == 38229);
    assert(wheel_position_display_rotation(82880) == -15001);
}

static void test_velocity(void) {
    WheelVelocityEstimator estimator;
    wheel_velocity_reset(&estimator);

    assert(wheel_velocity_update(&estimator, 132, 0) == -59);
    assert(wheel_velocity_update(&estimator, 264, 0) == -59);
    assert(wheel_velocity_update(&estimator, 264, 1) == -146);
    assert(wheel_velocity_update(&estimator, 264, 2) == -180);
    assert(wheel_velocity_update(&estimator, 0, 3) == -67);
}

int main(void) {
    test_centering();
    test_deadband();
    test_axis_scaling();
    test_invalid_travel();
    test_reference_capture();
    test_calibration_building();
    test_range_conversion();
    test_display_rotation();
    test_velocity();
    return 0;
}
