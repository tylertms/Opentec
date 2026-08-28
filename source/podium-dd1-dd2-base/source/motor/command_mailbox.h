#ifndef OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H
#define OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H

#include <stdint.h>

#include "transfer/command.h"

enum {
    MOTOR_COMMAND_MAILBOX_OWNER = 0x20,
    MOTOR_COMMAND_MAILBOX_REGISTER_SIZE = 4,
};

CommandTransportResult motor_command_mailbox_queue_payload(CommandTransport *transport,
                                                           const uint8_t *payload, uint16_t length);
CommandTransportResult
motor_command_mailbox_queue_control(CommandTransport *transport,
                                    const uint8_t control[MOTOR_COMMAND_MAILBOX_REGISTER_SIZE]);
CommandTransportResult
motor_command_mailbox_queue_command(CommandTransport *transport,
                                    const uint8_t command[MOTOR_COMMAND_MAILBOX_REGISTER_SIZE]);

#endif
