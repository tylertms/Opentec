#include "common/motor/encoder_calibration.h"

#include <assert.h>

static MotorEncoderCalibrationStep advance_settle(MotorEncoderCalibrationState *state,
                                                  MotorEncoderCalibrationInput *input) {
    MotorEncoderCalibrationStep step = {0};
    for (uint32_t index = 0U; index < 1000U; ++index) {
        step = motor_encoder_calibration_step(state, input);
    }
    return step;
}

static void test_capture_sequence(void) {
    MotorEncoderCalibrationState state;
    motor_encoder_calibration_initialize(&state);
    MotorEncoderCalibrationInput input = {
        .velocity = 327,
        .position = 12000,
        .relative_position = 100,
        .encoder_period = 0x5c80U,
    };

    MotorEncoderCalibrationStep step = motor_encoder_calibration_step(&state, &input);
    assert(step.reset_controller);
    step = motor_encoder_calibration_step(&state, &input);
    assert(step.drive_current == 327);
    step = advance_settle(&state, &input);
    assert(step.arm_revolution);

    input.relative_position = 110U;
    input.correction = 80;
    step = motor_encoder_calibration_step(&state, &input);
    assert(state.record.forward[11] == 10);
    input.relative_position = 100U;
    input.revolution_complete = true;
    step = motor_encoder_calibration_step(&state, &input);
    assert(step.clear_revolution);
    assert(step.drive_current == -327);

    input.revolution_complete = false;
    input.velocity = -327;
    step = advance_settle(&state, &input);
    assert(step.arm_revolution);
    input.relative_position = 120U;
    input.correction = -80;
    step = motor_encoder_calibration_step(&state, &input);
    assert(state.record.reverse[12] != 0);
    input.relative_position = 100U;
    input.revolution_complete = true;
    step = motor_encoder_calibration_step(&state, &input);
    assert(step.drive_current == -655);

    input.position = 100;
    input.revolution_complete = false;
    step = motor_encoder_calibration_step(&state, &input);
    assert(step.result == kMotorEncoderCalibrationComplete);
    assert(step.drive_current == 0);
    assert(state.record.magic == 0xaaaaaaaaU);
    assert(state.record.version == 3U);
    assert(state.record.correction_scale == 0x3333U);
    assert(state.record.sample_offset == 2U);
}

static void test_velocity_and_center_boundaries(void) {
    MotorEncoderCalibrationState state;
    motor_encoder_calibration_initialize(&state);
    MotorEncoderCalibrationInput input = {
        .velocity = 294,
        .position = 12000,
        .relative_position = 100U,
        .encoder_period = 0x5c80U,
    };
    (void)motor_encoder_calibration_step(&state, &input);
    (void)motor_encoder_calibration_step(&state, &input);
    (void)advance_settle(&state, &input);
    assert(state.phase == kMotorEncoderCalibrationSettleForward);

    state.phase = kMotorEncoderCalibrationCenter;
    state.drive_current = 655;
    input.position = -101;
    assert(motor_encoder_calibration_step(&state, &input).result ==
           kMotorEncoderCalibrationPending);
    input.position = -100;
    assert(motor_encoder_calibration_step(&state, &input).result ==
           kMotorEncoderCalibrationComplete);
}

static void test_correction_read(void) {
    MotorEncoderCalibrationRecord record = {
        .correction_scale = 0x00004000U,
        .sample_offset = 2U,
    };
    record.forward[2] = 100;
    record.forward[1] = 200;
    record.reverse[7] = -100;
    assert(motor_encoder_correction_read(&record, false, 0U, 10U) == 50);
    assert(motor_encoder_correction_read(&record, false, 80U, 10U) == 100);
    assert(motor_encoder_correction_read(&record, false, 90U, 10U) == 50);
    assert(motor_encoder_correction_read(&record, true, 0U, 10U) == -50);
}

static void test_persistent_record_validation(void) {
    MotorEncoderCalibrationRecord record = {
        .magic = UINT32_C(0xaaaaaaaa),
        .version = 3U,
    };

    assert(motor_encoder_calibration_record_is_valid(&record));

    record.magic = 0U;
    assert(!motor_encoder_calibration_record_is_valid(&record));

    record.magic = UINT32_C(0xaaaaaaaa);
    record.version = 2U;
    assert(!motor_encoder_calibration_record_is_valid(&record));
}

int main(void) {
    test_capture_sequence();
    test_velocity_and_center_boundaries();
    test_correction_read();
    test_persistent_record_validation();
    return 0;
}
