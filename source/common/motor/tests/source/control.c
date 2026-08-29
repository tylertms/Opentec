#include "common/motor/control.h"

#include <assert.h>

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

static void test_request_decode(void) {
    assert(motor_control_request_decode(0x1234aaaaU, 0U) == kMotorControlRequestCalibrateEncoder);
    assert(motor_control_request_decode(0xbbbbU, 0xabcdU) ==
           kMotorControlRequestEraseEncoderCalibration);
    assert(motor_control_request_decode(0U, 0x5678abcdU) ==
           kMotorControlRequestCheckEncoderDirection);
    assert(motor_control_request_decode(0U, 0U) == kMotorControlRequestNone);
}

static void test_startup_ramp(void) {
    assert(motor_control_startup_ramp_current(2000U) == 0U);
    assert(motor_control_startup_ramp_current(1999U) == 10U);
    assert(motor_control_startup_ramp_current(1001U) == 9990U);
    assert(motor_control_startup_ramp_current(1000U) == 10000U);
    assert(motor_control_startup_ramp_current(0U) == 10000U);
    assert(motor_control_startup_ramp_current(2001U) == 10000U);
}

int main(void) {
    test_startup_sequence();
    test_calibration_transitions();
    test_request_decode();
    test_startup_ramp();
    return 0;
}
