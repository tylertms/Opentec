#ifndef OPENTEC_BASE_MOTOR_COMMAND_SERIAL_H
#define OPENTEC_BASE_MOTOR_COMMAND_SERIAL_H

#include <stdbool.h>

#include "motor/serial_session.h"
#include "transfer/command.h"

bool motor_command_serial_submit(CommandTransport *transport, MotorSerialSession *session);
bool motor_command_serial_receive(CommandTransport *transport, MotorSerialSession *session);

#endif
