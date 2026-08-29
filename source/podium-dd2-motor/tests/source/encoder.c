#include "motor/encoder.h"

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

static void test_index_seek(void) {
    MotorEncoderIndexSeekStep pending = motor_encoder_index_seek_step(false, 5000U);
    assert(!pending.complete);
    assert(pending.countdown_active);
    assert(pending.drive_current == 491);

    MotorEncoderIndexSeekStep detected = motor_encoder_index_seek_step(true, 4999U);
    assert(detected.complete);
    assert(!detected.countdown_active);
    assert(detected.drive_current == 0);

    MotorEncoderIndexSeekStep timed_out = motor_encoder_index_seek_step(false, 0U);
    assert(timed_out.complete);
    assert(timed_out.drive_current == 0);
}

static void test_relative_position(void) {
    assert(motor_encoder_relative_position(500U, 100U, 0x5c7fU) == 400U);
    assert(motor_encoder_relative_position(100U, 500U, 0x5c7fU) == 0x5aefU);
}

static void test_direction_check_pass(void) {
    MotorEncoderDirectionState state;
    motor_encoder_direction_initialize(&state);

    MotorEncoderDirectionStep step = motor_encoder_direction_check_step(&state, false, 100, 0x5c7f);
    assert(step.result == kMotorEncoderDirectionPending);
    assert(step.restart_index_seek);
    assert(step.reset_controller);
    assert(step.reset_position);
    assert(step.status == 0xaaaaU);

    step = motor_encoder_direction_check_step(&state, false, 110, 0x5c7f);
    assert(step.drive_current == 491);

    step = motor_encoder_direction_check_step(&state, true, 120, 0x5c7f);
    assert(step.restart_index_seek);
    assert(!step.reset_controller);
    assert(step.reset_position);

    step = motor_encoder_direction_check_step(&state, false, 0x5c7f, 0x5c7f);
    assert(step.drive_current == 491);

    step = motor_encoder_direction_check_step(&state, true, 120 + 0x5c7f + 9, 0x5c7f);
    assert(step.result == kMotorEncoderDirectionPending);
    assert(state.phase == kMotorEncoderDirectionReturn);

    step = motor_encoder_direction_check_step(&state, false, 101, 0x5c7f);
    assert(step.drive_current == -491);
    step = motor_encoder_direction_check_step(&state, false, 100, 0x5c7f);
    assert(step.result == kMotorEncoderDirectionPassed);
    assert(step.status == 0U);
}

static void test_direction_check_failure(void) {
    MotorEncoderDirectionState state;
    motor_encoder_direction_initialize(&state);
    (void)motor_encoder_direction_check_step(&state, false, 100, 0x5c7f);
    (void)motor_encoder_direction_check_step(&state, true, 120, 0x5c7f);

    MotorEncoderDirectionStep step =
        motor_encoder_direction_check_step(&state, true, 120 + 0x5c7f + 10, 0x5c7f);
    assert(step.result == kMotorEncoderDirectionFailed);
    assert(step.status == 0xbbbbU);
    assert(state.phase == kMotorEncoderDirectionBegin);
}

int main(void) {
    test_overflow_extension();
    test_position_update();
    test_reset();
    test_index_seek();
    test_relative_position();
    test_direction_check_pass();
    test_direction_check_failure();
    return 0;
}
