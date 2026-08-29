#include "common/motor/encoder.h"

#include <assert.h>

static void test_overflow_extension(void) {
    MotorEncoderState state = {0};
    motor_encoder_overflow_apply(&state, 0x5c7f, true);
    assert(state.revolution_offset == 0x5c7f);
    motor_encoder_overflow_apply(&state, 0x5c7f, false);
    assert(state.revolution_offset == 0);
    motor_encoder_overflow_apply(&state, 0x5c7f, false);
    assert(state.revolution_offset == -0x5c7f);
}

static void test_position_update(void) {
    MotorEncoderState state = {
        .revolution_offset = 1000,
        .position = 77,
        .zero_counter = 500,
    };
    assert(motor_encoder_position_update(&state, true, 800U, 2000) == kMotorEncoderPositionPending);
    assert(state.position == 77);

    assert(motor_encoder_position_update(&state, false, 800U, 2000) ==
           kMotorEncoderPositionUpdated);
    assert(state.position == 1300);
    assert(motor_encoder_position_update(&state, false, 1500U, 2000) ==
           kMotorEncoderPositionOutOfRange);
    assert(state.position == 2000);

    state.revolution_offset = -2000;
    state.zero_counter = 0;
    assert(motor_encoder_position_update(&state, false, 0U, 2000) ==
           kMotorEncoderPositionOutOfRange);
}

static void test_reset(void) {
    MotorEncoderState state = {
        .revolution_offset = 100,
        .position = 200,
        .zero_counter = 300,
    };
    motor_encoder_position_reset(&state);
    assert(state.revolution_offset == 0);
    assert(state.position == 0);
    assert(state.zero_counter == 300);
}

int main(void) {
    test_overflow_extension();
    test_position_update();
    test_reset();
    return 0;
}
