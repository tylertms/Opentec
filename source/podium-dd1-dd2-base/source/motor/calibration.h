#ifndef OPENTEC_BASE_MOTOR_CALIBRATION_H
#define OPENTEC_BASE_MOTOR_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/telemetry.h"
#include "usb/output_command.h"

typedef enum {
    MOTOR_CALIBRATION_OPERATION_CALIBRATE,
    MOTOR_CALIBRATION_OPERATION_ERASE,
} MotorCalibrationOperation;

typedef enum {
    MOTOR_CALIBRATION_IDLE,
    MOTOR_CALIBRATION_WRITE_COMMAND,
    MOTOR_CALIBRATION_READ_RESPONSE,
} MotorCalibrationPhase;

typedef enum {
    MOTOR_CALIBRATION_EVENT_NONE = 0,
    MOTOR_CALIBRATION_EVENT_DISCONNECT_WHEEL = 8,
    MOTOR_CALIBRATION_EVENT_UNSUPPORTED = 9,
    MOTOR_CALIBRATION_EVENT_COMPLETED = 10,
    MOTOR_CALIBRATION_EVENT_ERASED = 11,
    MOTOR_CALIBRATION_EVENT_STARTED = 0x1c,
} MotorCalibrationEvent;

typedef struct {
    MotorCalibrationPhase phase;
    MotorCalibrationOperation operation;
    MotorCalibrationEvent event;
    uint8_t data[2];
    uint8_t requests;
    bool transfer_active;
} MotorCalibrationService;

bool motor_calibration_command_decode(const UsbOutputCommand *output,
                                      MotorCalibrationOperation *operation);
void motor_calibration_service_init(MotorCalibrationService *service);
void motor_calibration_service_request(MotorCalibrationService *service,
                                       MotorCalibrationOperation operation);
void motor_calibration_service_run(MotorCalibrationService *service, uint8_t wheel_mode,
                                   const MotorTelemetry *telemetry);
bool motor_calibration_service_pending(const MotorCalibrationService *service);
bool motor_calibration_service_owns_bus(const MotorCalibrationService *service);
MotorCalibrationEvent motor_calibration_service_take_event(MotorCalibrationService *service);

#endif
