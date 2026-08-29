#include "common/motor/control.h"

enum {
    MOTOR_ENCODER_CALIBRATION_COMMAND = 0xaaaaU,
    MOTOR_ENCODER_CALIBRATION_ERASE_COMMAND = 0xbbbbU,
    MOTOR_ENCODER_DIRECTION_CHECK_COMMAND = 0xabcdU,
    MOTOR_STARTUP_RAMP_TICKS = 2000U,
    MOTOR_STARTUP_RAMP_STEP = 10U,
    MOTOR_STARTUP_RAMP_LIMIT = 10000U,
};

/**
 * @brief Selects the official first motor startup interlock state.
 * @return Initial motor control mode.
 */
MotorControlMode motor_control_mode_initialize(void) { return kMotorControlStartupInterlockA; }

/**
 * @brief Advances one completed official motor control mode.
 * @param mode Current motor control mode.
 * @return Next mode, or the current mode when it has no completion transition.
 */
MotorControlMode motor_control_mode_complete(MotorControlMode mode) {
    switch (mode) {
    case kMotorControlStartupInterlockA:
        return kMotorControlStartupInterlockB;
    case kMotorControlStartupInterlockB:
        return kMotorControlCurrentCalibration;
    case kMotorControlCurrentCalibration:
        return kMotorControlStartupRamp;
    case kMotorControlStartupRamp:
        return kMotorControlStartupGate;
    case kMotorControlEncoderCalibration:
    case kMotorControlStartupGate:
    case kMotorControlEncoderDirectionCheck:
        return kMotorControlRun;
    default:
        return mode;
    }
}

/**
 * @brief Applies an official run-mode calibration request to the control mode.
 * @param mode Current motor control mode.
 * @param request Decoded calibration request.
 * @return Requested calibration mode, or the unchanged current mode.
 */
MotorControlMode motor_control_request_apply(MotorControlMode mode, MotorControlRequest request) {
    if (mode != kMotorControlRun) {
        return mode;
    }

    if (request == kMotorControlRequestCalibrateEncoder) {
        return kMotorControlEncoderCalibration;
    }
    if (request == kMotorControlRequestCheckEncoderDirection) {
        return kMotorControlEncoderDirectionCheck;
    }
    return mode;
}

/**
 * @brief Decodes the official run-mode encoder calibration command words.
 * @param calibration_command Encoder calibration or erase command word.
 * @param direction_command Encoder direction-check command word.
 * @return Highest-priority recognized request.
 */
MotorControlRequest motor_control_request_decode(uint32_t calibration_command,
                                                 uint32_t direction_command) {
    uint16_t calibration = (uint16_t)calibration_command;
    if (calibration == MOTOR_ENCODER_CALIBRATION_COMMAND) {
        return kMotorControlRequestCalibrateEncoder;
    }
    if (calibration == MOTOR_ENCODER_CALIBRATION_ERASE_COMMAND) {
        return kMotorControlRequestEraseEncoderCalibration;
    }
    if ((uint16_t)direction_command == MOTOR_ENCODER_DIRECTION_CHECK_COMMAND) {
        return kMotorControlRequestCheckEncoderDirection;
    }
    return kMotorControlRequestNone;
}

/**
 * @brief Resolves the official startup current ramp from its service countdown.
 * @param ticks_remaining Countdown initialized to two thousand service ticks.
 * @return Current command rising by ten per elapsed tick and limited to ten thousand.
 */
uint16_t motor_control_startup_ramp_current(uint16_t ticks_remaining) {
    uint32_t elapsed = (uint16_t)(MOTOR_STARTUP_RAMP_TICKS - ticks_remaining);
    uint32_t current = elapsed * MOTOR_STARTUP_RAMP_STEP;
    return (uint16_t)(current < MOTOR_STARTUP_RAMP_LIMIT ? current : MOTOR_STARTUP_RAMP_LIMIT);
}
