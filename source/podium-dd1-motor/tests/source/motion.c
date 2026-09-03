#include "motor/motion.h"

#include <assert.h>
#include <limits.h>

static void test_fixed_point_scaling(void) {
    assert(motor_q15_scale_saturate(0x002985a1U, 1) == 83);
    assert(motor_q15_scale_saturate(0x002985a1U, -1) == -84);
    assert(motor_q15_scale_saturate(0x002985a1U, 1000) == INT16_MAX);
    assert(motor_q15_scale_saturate(0x002985a1U, -1000) == INT16_MIN);
    assert(motor_q15_scale_saturate(0x3fff8000U, 1) == INT16_MAX);
    assert(motor_q15_scale_saturate(0x40000000U, 1) == INT16_MIN);
    assert(motor_q15_scale_saturate(0x40008000U, 1) == INT16_MAX);
    assert(motor_q15_scale_saturate(0x40010000U, 1) == INT16_MAX);
    assert(motor_q15_scale_saturate(0xc0000000U, 1) == INT16_MIN);
    assert(motor_q15_scale_saturate(0xbfff0000U, 1) == INT16_MIN);
    assert(motor_q15_scale_saturate(0x80000000U, INT16_MIN) == INT16_MAX);
    assert(motor_q15_scale_saturate(0x80000001U, INT16_MIN) == INT16_MAX);
    assert(motor_q15_scale_wrap(0x000f3851U, 1) == 30);
    assert(motor_q15_scale_wrap(0x000f3851U, -1) == -31);
    assert(motor_q15_scale_wrap(0x000f3851U, 0x5c7f) == -108);
    assert(motor_q15_scale_wrap(0x000f1ca2U, 0x5d2b) == -31);
    assert(motor_q15_scale_wrap(0x75c2U, 1000) == 919);
    assert(motor_q15_scale_wrap(0x75c2U, -1000) == -920);
}

static void test_saturated_difference(void) {
    assert(motor_signed_difference_saturate(INT16_MAX, INT16_MIN) == INT16_MAX);
    assert(motor_signed_difference_saturate(INT16_MIN, INT16_MAX) == INT16_MIN);
    assert(motor_signed_difference_saturate(123, 100) == 23);
}

static void test_filter(void) {
    MotorMotionFilter positive = {.shift = 4U};
    assert(motor_motion_filter_step(&positive, 160) == 10);
    assert(positive.accumulator == 150);
    assert(motor_motion_filter_step(&positive, 0) == 9);
    assert(positive.accumulator == 141);

    MotorMotionFilter negative = {.shift = 4U};
    assert(motor_motion_filter_step(&negative, -160) == -10);
    assert(negative.accumulator == -150);

    MotorMotionFilter upper = {.accumulator = INT32_MAX, .shift = 0U};
    assert(motor_motion_filter_step(&upper, 1) == INT16_MAX);
    MotorMotionFilter lower = {.accumulator = INT32_MIN, .shift = 0U};
    assert(motor_motion_filter_step(&lower, -1) == INT16_MIN);
}

static void test_estimator(void) {
    MotorMotionState state = {.previous_counter = 1000U};
    MotorMotionFilter position_filter = {.shift = 4U};
    MotorMotionFilter velocity_filter = {.shift = 6U};

    MotorMotionSample first =
        motor_motion_sample(&state, &position_filter, &velocity_filter, 1001U, 0x002985a1U);
    assert(first.position_delta == 83);
    assert(first.filtered_position_delta == 5);
    assert(first.velocity_delta == 415);
    assert(first.filtered_velocity_delta == 6);

    MotorMotionSample second =
        motor_motion_sample(&state, &position_filter, &velocity_filter, 1002U, 0x002985a1U);
    assert(second.position_delta == 83);
    assert(second.filtered_position_delta == 10);
    assert(second.velocity_delta == 415);
    assert(second.filtered_velocity_delta == 12);

    state.previous_counter = UINT16_MAX;
    assert(motor_encoder_delta_scale(&state, 0U, 0x002985a1U) == 83);
}

int motor_test_motion(void) {
    test_fixed_point_scaling();
    test_saturated_difference();
    test_filter();
    test_estimator();
    return 0;
}
