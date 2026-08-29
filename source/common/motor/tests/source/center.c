#include "common/motor/center.h"

#include <assert.h>

static void test_inactive_and_unchanged(void) {
    MotorCenterState state = {.requested = 10, .encoder_offset = 20};
    assert(!motor_center_command_apply(&state, 11, 100, 0U, 50U));
    assert(state.requested == 10);

    state.active = true;
    assert(!motor_center_command_apply(&state, 10, 100, 0U, 50U));
}

static void test_offset_clamp(void) {
    MotorCenterState positive = {.encoder_offset = 101, .active = true};
    assert(motor_center_command_apply(&positive, 1, 100, 0U, 50U));
    assert(positive.encoder_offset == 100);

    MotorCenterState negative = {.encoder_offset = -101, .active = true};
    assert(motor_center_command_apply(&negative, 1, 100, 100U, 50U));
    assert(negative.encoder_offset == -100);
}

static void test_shared_endpoint(void) {
    MotorCenterState negative = {.encoder_offset = -100, .active = true};
    assert(motor_center_command_apply(&negative, 1, 100, 49U, 50U));
    assert(negative.encoder_offset == 0);

    MotorCenterState positive = {.encoder_offset = 100, .active = true};
    assert(motor_center_command_apply(&positive, 1, 100, 50U, 50U));
    assert(positive.encoder_offset == 0);

    MotorCenterState retained_negative = {.encoder_offset = -100, .active = true};
    assert(motor_center_command_apply(&retained_negative, 1, 100, 50U, 50U));
    assert(retained_negative.encoder_offset == -100);

    MotorCenterState retained_positive = {.encoder_offset = 100, .active = true};
    assert(motor_center_command_apply(&retained_positive, 1, 100, 49U, 50U));
    assert(retained_positive.encoder_offset == 100);
}

static void test_centered_position(void) {
    assert(motor_centered_position_resolve(1000, 250) == 750);
    assert(motor_centered_position_resolve(100000, -1000) == 82880);
    assert(motor_centered_position_resolve(-100000, 1000) == -82880);
}

int main(void) {
    test_inactive_and_unchanged();
    test_offset_clamp();
    test_shared_endpoint();
    test_centered_position();
    return 0;
}
