#ifndef OPENTEC_BASE_MOTOR_COMMAND_SERIAL_H
#define OPENTEC_BASE_MOTOR_COMMAND_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/service.h"
#include "transfer/command.h"

bool motor_command_serial_submit(CommandTransport *transport, SerialService *service,
                                 uint32_t now_ms);
bool motor_command_serial_receive(CommandTransport *transport, SerialService *service);

#endif
