#ifndef OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H
#define OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

enum {
    MOTOR_COMMAND_MAILBOX_OWNER = 0x20,
    MOTOR_COMMAND_MAILBOX_CONTROL_SIZE = 4,
    MOTOR_COMMAND_MAILBOX_LENGTH_SIZE = 2,
    MOTOR_COMMAND_MAILBOX_STATUS_SIZE = 4,
};

typedef struct {
    uint16_t payload_length;
    uint8_t flags;
    uint8_t reserved;
} MotorCommandMailboxControl;

CommandTransportResult motor_command_mailbox_queue_payload_read(CommandTransport *transport,
                                                                uint8_t *payload, uint16_t length);
CommandTransportResult motor_command_mailbox_queue_payload_write(CommandTransport *transport,
                                                                 const uint8_t *payload,
                                                                 uint16_t length);
CommandTransportResult
motor_command_mailbox_queue_length_read(CommandTransport *transport,
                                        uint8_t length[MOTOR_COMMAND_MAILBOX_LENGTH_SIZE]);
CommandTransportResult
motor_command_mailbox_queue_control_read(CommandTransport *transport,
                                         uint8_t control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE]);
CommandTransportResult
motor_command_mailbox_queue_status_read(CommandTransport *transport,
                                        uint8_t status[MOTOR_COMMAND_MAILBOX_STATUS_SIZE]);
bool motor_command_mailbox_control_decode(const uint8_t record[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE],
                                          MotorCommandMailboxControl *control);

#endif
