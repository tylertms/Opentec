#ifndef OPENTEC_MOTOR_COMMUNICATION_H
#define OPENTEC_MOTOR_COMMUNICATION_H

#include "tuning/parameter.h"

typedef void (*MotorParameterChangedHandler)(void *context);

void motor_bus_initialize(MotorParameterBank *parameters,
                          MotorParameterChangedHandler changed_handler, void *context);
void motor_bus_service(void);

#endif
