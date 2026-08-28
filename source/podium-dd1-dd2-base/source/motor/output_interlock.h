#ifndef OPENTEC_BASE_MOTOR_OUTPUT_INTERLOCK_H
#define OPENTEC_BASE_MOTOR_OUTPUT_INTERLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"

typedef struct {
    bool engaged;
} MotorOutputInterlock;

void motor_output_interlock_init(MotorOutputInterlock *interlock);
void motor_output_interlock_accept_command(MotorOutputInterlock *interlock,
                                           const MotorIdentity *identity, uint16_t response);
void motor_output_interlock_accept_status(MotorOutputInterlock *interlock,
                                          const MotorIdentity *identity, uint8_t response);
bool motor_output_interlock_engaged(const MotorOutputInterlock *interlock);

#endif
