#include "motor/control.h"

#include <assert.h>
#include <limits.h>

static void test_startup_sequence(void) {
    MotorControlMode mode = motor_control_mode_initialize();
    assert(mode == kMotorControlStartupInterlockA);
    mode = motor_control_mode_complete(mode);
    assert(mode == kMotorControlStartupInterlockB);
    mode = motor_control_mode_complete(mode);
    assert(mode == kMotorControlCurrentCalibration);
    mode = motor_control_mode_complete(mode);
    assert(mode == kMotorControlStartupRamp);
    mode = motor_control_mode_complete(mode);
    assert(mode == kMotorControlStartupGate);
    mode = motor_control_mode_complete(mode);
    assert(mode == kMotorControlRun);
    assert(motor_control_mode_complete(mode) == kMotorControlRun);
}

static void test_calibration_transitions(void) {
    assert(motor_control_mode_complete(kMotorControlEncoderCalibration) == kMotorControlRun);
    assert(motor_control_mode_complete(kMotorControlEncoderDirectionCheck) == kMotorControlRun);
    assert(motor_control_request_apply(kMotorControlStartupRamp,
                                       kMotorControlRequestCalibrateEncoder) ==
           kMotorControlStartupRamp);
    assert(motor_control_request_apply(kMotorControlRun, kMotorControlRequestCalibrateEncoder) ==
           kMotorControlEncoderCalibration);
    assert(
        motor_control_request_apply(kMotorControlRun, kMotorControlRequestCheckEncoderDirection) ==
        kMotorControlEncoderDirectionCheck);
}

static void test_maintenance_request_preserves_run_mode(void) {
    assert(motor_control_request_apply(
               kMotorControlRun, kMotorControlRequestEraseEncoderCalibration) == kMotorControlRun);
}

static void test_request_decode(void) {
    assert(motor_control_request_decode(0x1234aaaaU, 0U) == kMotorControlRequestCalibrateEncoder);
    assert(motor_control_request_decode(0xbbbbU, 0xabcdU) ==
           kMotorControlRequestEraseEncoderCalibration);
    assert(motor_control_request_decode(0U, 0x5678abcdU) ==
           kMotorControlRequestCheckEncoderDirection);
    assert(motor_control_request_decode(0U, 0U) == kMotorControlRequestNone);
}

static void test_control_update_cadence(void) {
    uint8_t conversion_count = 0U;
    for (uint32_t index = 0U; index < 6U; ++index) {
        assert(!motor_control_update_due(&conversion_count));
    }
    assert(conversion_count == 6U);
    assert(motor_control_update_due(&conversion_count));
    assert(conversion_count == 0U);
    assert(!motor_control_update_due(&conversion_count));
}

static void test_startup_ramp(void) {
    assert(motor_control_startup_ramp_current(2000U) == 0U);
    assert(motor_control_startup_ramp_current(1999U) == 10U);
    assert(motor_control_startup_ramp_current(1001U) == 9990U);
    assert(motor_control_startup_ramp_current(1000U) == 10000U);
    assert(motor_control_startup_ramp_current(0U) == 10000U);
    assert(motor_control_startup_ramp_current(2001U) == 10000U);
}

static void test_current_reference(void) {
    MotorControlCurrentReference reference = motor_control_current_reference(1000);
    assert(reference.d == -1000);
    assert(reference.q == 1000);

    reference = motor_control_current_reference(-1000);
    assert(reference.d == -1000);
    assert(reference.q == -1000);

    reference = motor_control_current_reference(10000);
    assert(reference.d == -0x1999);
    assert(reference.q == 10000);

    reference = motor_control_current_reference(INT16_MIN);
    assert(reference.d == INT16_MIN);
    assert(reference.q == INT16_MIN);
}

int motor_test_control(void) {
    test_startup_sequence();
    test_calibration_transitions();
    test_maintenance_request_preserves_run_mode();
    test_request_decode();
    test_control_update_cadence();
    test_startup_ramp();
    test_current_reference();
    return 0;
}
