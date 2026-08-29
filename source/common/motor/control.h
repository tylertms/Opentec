#ifndef OPENTEC_MOTOR_CONTROL_H
#define OPENTEC_MOTOR_CONTROL_H

#include <stdint.h>

typedef enum {
    kMotorControlInactive = 0,
    kMotorControlStartupInterlockA = 1,
    kMotorControlStartupInterlockB = 2,
    kMotorControlCurrentCalibration = 3,
    kMotorControlStartupRamp = 4,
    kMotorControlEncoderCalibration = 5,
    kMotorControlStartupGate = 6,
    kMotorControlRun = 7,
    kMotorControlEncoderDirectionCheck = 8,
} MotorControlMode;

typedef enum {
    kMotorControlRequestNone,
    kMotorControlRequestCalibrateEncoder,
    kMotorControlRequestEraseEncoderCalibration,
    kMotorControlRequestCheckEncoderDirection,
} MotorControlRequest;

typedef struct {
    int16_t d;
    int16_t q;
} MotorControlCurrentReference;

MotorControlMode motor_control_mode_initialize(void);
MotorControlMode motor_control_mode_complete(MotorControlMode mode);
MotorControlMode motor_control_request_apply(MotorControlMode mode, MotorControlRequest request);
MotorControlRequest motor_control_request_decode(uint32_t calibration_command,
                                                 uint32_t direction_command);
uint16_t motor_control_startup_ramp_current(uint16_t ticks_remaining);
MotorControlCurrentReference motor_control_current_reference(int16_t torque_current);

#endif
