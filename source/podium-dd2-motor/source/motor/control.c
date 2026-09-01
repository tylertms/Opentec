#include "motor/control.h"

/** @brief Protocol command words and startup-control constants. */
enum {
    MOTOR_ENCODER_CALIBRATION_COMMAND = 0xaaaaU, /**< Encoder calibration command word. */
    MOTOR_ENCODER_CALIBRATION_ERASE_COMMAND = 0xbbbbU, /**< Encoder-calibration erase command. */
    MOTOR_ENCODER_DIRECTION_CHECK_COMMAND = 0xabcdU, /**< Encoder direction-check command word. */
    MOTOR_STARTUP_RAMP_TICKS = 2000U, /**< Number of startup-ramp countdown ticks. */
    MOTOR_STARTUP_RAMP_STEP = 10U, /**< Current counts added per elapsed ramp tick. */
    MOTOR_STARTUP_RAMP_LIMIT = 10000U, /**< Maximum startup-ramp current command. */
    MOTOR_D_AXIS_CURRENT_LIMIT = 0x1999, /**< Absolute D-axis current limit. */
};

/**
 * @brief Selects the official first motor startup interlock state.
 *
 * The returned mode is the first state consumed by the startup state machine.
 *
 * @return Initial motor control mode.
 */
MotorControlMode motor_control_mode_initialize(void) { return kMotorControlStartupInterlockA; }

/**
 * @brief Advances one completed official motor control mode.
 *
 * Startup phases advance in order; encoder calibration, startup gating, and direction diagnostics
 * return to run mode when complete.
 *
 * @param[in] mode Current motor control mode.
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
 * @brief Applies an official run-mode start request to the control mode.
 *
 * Encoder-calibration and direction-check start requests are accepted only while run mode is active.
 *
 * @param[in] mode Current motor control mode.
 * @param[in] request Decoded calibration or direction-check request.
 * @return Requested calibration or diagnostic mode, or the unchanged current mode.
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
 * @brief Decodes the official encoder calibration and direction-check command words.
 *
 * Calibration and erase commands take priority over the independent direction diagnostic.
 *
 * @param[in] calibration_command Encoder calibration or erase command word.
 * @param[in] direction_command Encoder direction-check command word.
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
 * @brief Advances the seven-interrupt control-update cadence.
 *
 * The divider raises one deferred control update after every seventh completed ADC interrupt.
 *
 * @param[in,out] conversion_count Persistent interrupt divider from zero through six.
 * @return True when the deferred control update is due.
 */
bool motor_control_update_due(uint8_t *conversion_count) {
    uint8_t previous = *conversion_count;
    *conversion_count = (uint8_t)(previous + 1U);
    if (previous <= 5U) {
        return false;
    }

    *conversion_count = 0U;
    return true;
}

/**
 * @brief Resolves the official startup current ramp from its service countdown.
 *
 * Elapsed ticks add ten current counts until the ten-thousand-count alignment limit is reached.
 *
 * @param[in] ticks_remaining Countdown initialized to two thousand service ticks.
 * @return Current command rising by ten per elapsed tick and limited to ten thousand.
 */
uint16_t motor_control_startup_ramp_current(uint16_t ticks_remaining) {
    uint32_t elapsed = (uint16_t)(MOTOR_STARTUP_RAMP_TICKS - ticks_remaining);
    uint32_t current = elapsed * MOTOR_STARTUP_RAMP_STEP;
    return (uint16_t)(current < MOTOR_STARTUP_RAMP_LIMIT ? current : MOTOR_STARTUP_RAMP_LIMIT);
}

/**
 * @brief Resolves the official D/Q references from a signed torque-current command.
 *
 * Torque remains on the Q axis while its representable magnitude is capped for the negative D-axis
 * command; the most-negative input retains its wrapped signed representation.
 *
 * @param[in] torque_current Signed Q-axis torque-current command.
 * @return Q-axis torque current and negative D-axis current reference.
 */
MotorControlCurrentReference motor_control_current_reference(int16_t torque_current) {
    uint16_t sign = torque_current < 0 ? UINT16_MAX : 0U;
    int16_t magnitude = (int16_t)(((uint16_t)torque_current + sign) ^ sign);
    if (magnitude > MOTOR_D_AXIS_CURRENT_LIMIT) {
        magnitude = MOTOR_D_AXIS_CURRENT_LIMIT;
    }

    return (MotorControlCurrentReference){
        .d = (int16_t)-magnitude,
        .q = torque_current,
    };
}
