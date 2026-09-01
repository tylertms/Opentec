#include "motor/center.h"

#include <assert.h>

static void test_inactive_and_unchanged(void) {
    MotorCenterState state = {.requested = 10};
    int32_t encoder_offset = 20;
    uint32_t counter = 0U;
    assert(!motor_center_command_apply(&state, 11, 100, &counter, 50U, &encoder_offset));
    assert(state.requested == 10);

    state.active = true;
    assert(!motor_center_command_apply(&state, 10, 100, &counter, 50U, &encoder_offset));
}

static void test_offset_clamp(void) {
    MotorCenterState positive = {.active = true};
    int32_t positive_offset = 101;
    uint32_t counter = 0U;
    assert(motor_center_command_apply(&positive, 1, 100, &counter, 50U, &positive_offset));
    assert(positive_offset == 100);

    MotorCenterState negative = {.active = true};
    int32_t negative_offset = -101;
    counter = 100U;
    assert(motor_center_command_apply(&negative, 1, 100, &counter, 50U, &negative_offset));
    assert(negative_offset == -100);
}

static void test_shared_endpoint(void) {
    MotorCenterState negative = {.active = true};
    int32_t negative_offset = -100;
    uint32_t counter = 49U;
    assert(motor_center_command_apply(&negative, 1, 100, &counter, 50U, &negative_offset));
    assert(negative_offset == 0);

    MotorCenterState positive = {.active = true};
    int32_t positive_offset = 100;
    counter = 50U;
    assert(motor_center_command_apply(&positive, 1, 100, &counter, 50U, &positive_offset));
    assert(positive_offset == 0);

    MotorCenterState retained_negative = {.active = true};
    int32_t retained_negative_offset = -100;
    assert(motor_center_command_apply(&retained_negative, 1, 100, &counter, 50U,
                                      &retained_negative_offset));
    assert(retained_negative_offset == -100);

    MotorCenterState retained_positive = {.active = true};
    int32_t retained_positive_offset = 100;
    counter = 49U;
    assert(motor_center_command_apply(&retained_positive, 1, 100, &counter, 50U,
                                      &retained_positive_offset));
    assert(retained_positive_offset == 100);
}

static void test_centered_position(void) {
    assert(motor_centered_position_resolve(1000, 250) == 750);
    assert(motor_centered_position_resolve(100000, -1000) == 82880);
    assert(motor_centered_position_resolve(-100000, 1000) == -82880);
}

int motor_test_center(void) {
    test_inactive_and_unchanged();
    test_offset_clamp();
    test_shared_endpoint();
    test_centered_position();
    return 0;
}
