#ifndef OPENTEC_MOTOR_CONTROL_H
#define OPENTEC_MOTOR_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Operating modes used by the motor-control state machine. */
typedef enum {
    kMotorControlInactive = 0, /**< Motor outputs are inactive. */
    kMotorControlStartupInterlockA = 1, /**< First startup interlock is active. */
    kMotorControlStartupInterlockB = 2, /**< Second startup interlock is active. */
    kMotorControlCurrentCalibration = 3, /**< Phase-current offset calibration is active. */
    kMotorControlStartupRamp = 4, /**< Startup current ramp is active. */
    kMotorControlEncoderCalibration = 5, /**< Encoder correction calibration is active. */
    kMotorControlStartupGate = 6, /**< Startup gate waits for the encoder index. */
    kMotorControlRun = 7, /**< Normal motor control is active. */
    kMotorControlEncoderDirectionCheck = 8, /**< Encoder direction diagnostic is active. */
} MotorControlMode;

/** @brief Requests accepted by the motor-control command decoder. */
typedef enum {
    kMotorControlRequestNone, /**< No recognized control request. */
    kMotorControlRequestCalibrateEncoder, /**< Start encoder correction calibration. */
    kMotorControlRequestEraseEncoderCalibration, /**< Erase stored encoder calibration. */
    kMotorControlRequestCheckEncoderDirection, /**< Start the encoder direction diagnostic. */
} MotorControlRequest;

/** @brief D-axis and Q-axis current references for the field-oriented controller. */
typedef struct {
    int16_t d; /**< Signed D-axis current reference. */
    int16_t q; /**< Signed Q-axis torque-current reference. */
} MotorControlCurrentReference;

/**
 * @brief Selects the first motor startup interlock mode.
 *
 * The returned mode is the first state consumed by the startup state machine.
 *
 * @return Initial motor-control mode.
 */
MotorControlMode motor_control_mode_initialize(void);

/**
 * @brief Advances a completed motor-control mode.
 *
 * Startup modes advance to their next phase; encoder calibration, startup gating, and direction
 * diagnostics return to run mode when complete.
 *
 * @param[in] mode Completed motor-control mode.
 * @return Next mode, or mode when no transition is defined.
 */
MotorControlMode motor_control_mode_complete(MotorControlMode mode);

/**
 * @brief Applies a decoded request to the active motor-control mode.
 *
 * Encoder-calibration and direction-check start requests are accepted only while run mode is
 * active.
 *
 * @param[in] mode Current motor-control mode.
 * @param[in] request Decoded motor-control request.
 * @return Requested calibration or diagnostic mode, or the unchanged current mode.
 */
MotorControlMode motor_control_request_apply(MotorControlMode mode, MotorControlRequest request);

/**
 * @brief Decodes calibration, erase, and direction command words.
 *
 * Calibration and erase words take priority over the independent direction-check word.
 *
 * @param[in] calibration_command Calibration or erase command word.
 * @param[in] direction_command Encoder direction-check command word.
 * @return Highest-priority recognized request.
 */
MotorControlRequest motor_control_request_decode(uint32_t calibration_command,
                                                 uint32_t direction_command);

/**
 * @brief Advances the deferred control-update divider.
 *
 * One deferred update is reported after every seventh completed ADC interrupt.
 *
 * @param[in,out] conversion_count Persistent interrupt divider.
 * @return True when a deferred control update is due.
 */
bool motor_control_update_due(uint8_t *conversion_count);

/**
 * @brief Resolves startup current from the remaining ramp countdown.
 *
 * The current increases by ten counts per elapsed tick and is limited to 10,000 counts.
 *
 * @param[in] ticks_remaining Remaining startup-ramp ticks.
 * @return Startup current command.
 */
uint16_t motor_control_startup_ramp_current(uint16_t ticks_remaining);

/**
 * @brief Builds D-axis and Q-axis references from a torque-current command.
 *
 * The Q-axis retains the signed command while its representable magnitude is capped for the
 * negative D-axis reference; INT16_MIN retains its wrapped signed representation.
 *
 * @param[in] torque_current Signed Q-axis torque-current command.
 * @return Negative D-axis and signed Q-axis current references.
 */
MotorControlCurrentReference motor_control_current_reference(int16_t torque_current);

#endif
