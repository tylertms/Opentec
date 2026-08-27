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
    assert(wheel_position_reference_capture(&reference, 25000));
    assert(reference.calibrated);
    assert(reference.center == 25000);
    assert(!wheel_position_reference_capture(&reference, 25000));
    assert(wheel_position_reference_capture(&reference, -25000));
    assert(reference.center == -25000);
}

static void test_calibration_building(void) {
    WheelPositionReference reference = {.center = 1234, .calibrated = true};
    WheelPositionCalibration calibration = wheel_position_calibration_build(&reference, 900, 4);
    assert(calibration.center == 1234);
    assert(calibration.travel == 30000);
    assert(calibration.deadband == 40);

    calibration = wheel_position_calibration_build(&reference, 2520, 0);
    assert(calibration.travel == WHEEL_POSITION_SAMPLE_LIMIT);

    reference.calibrated = false;
    calibration = wheel_position_calibration_build(&reference, 900, 0);
    assert(calibration.travel == 0);
    assert(wheel_position_hid_axis(INT32_MAX, &calibration) == 32768);
}

static void test_velocity(void) {
    WheelVelocityEstimator estimator;
    wheel_velocity_reset(&estimator);

    assert(wheel_velocity_update(&estimator, 100, UINT32_MAX - 1, 25) == 0);
    assert(wheel_velocity_update(&estimator, 100, UINT32_MAX - 1, 25) == 0);
    assert(wheel_velocity_update(&estimator, 110, 0, 25) == 1250);
    assert(wheel_velocity_update(&estimator, 120, 2, 25) == 2187);
    assert(wheel_velocity_update(&estimator, 100, 4, 100) == -10000);
    assert(wheel_velocity_update(&estimator, 90, 5, 0) == -10000);
}

int main(void) {
    test_centering();
    test_deadband();
    test_axis_scaling();
    test_invalid_travel();
    test_reference_capture();
    test_calibration_building();
    test_velocity();
    return 0;
}
