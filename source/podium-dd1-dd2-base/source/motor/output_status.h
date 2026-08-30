#ifndef OPENTEC_BASE_MOTOR_OUTPUT_STATUS_H
#define OPENTEC_BASE_MOTOR_OUTPUT_STATUS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool direct_force;
    bool xbox_mode;
    bool force_enabled;
    bool override_active;
    bool transition_active;
    bool primary_disabled;
    bool secondary_disabled;
    bool usb_disconnected;
    bool full_torque;
} MotorOutputStatusInput;

typedef struct {
    uint8_t value;
} MotorOutputStatus;

void motor_output_status_init(MotorOutputStatus *status);
uint8_t motor_output_status_update(MotorOutputStatus *status, const MotorOutputStatusInput *input);

#endif
