#ifndef OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H
#define OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H

#include <stdint.h>

#include "transfer/command.h"

enum {
    MOTOR_COMMAND_MAILBOX_OWNER = 0x20,
    MOTOR_COMMAND_MAILBOX_REGISTER_SIZE = 4,
    MOTOR_COMMAND_MAILBOX_STATUS_SIZE = 2,
};

typedef enum {
    MOTOR_COMMAND_MAILBOX_STATUS_WRITE,
    MOTOR_COMMAND_MAILBOX_CONTROL_WRITE,
    MOTOR_COMMAND_MAILBOX_COMMAND_WRITE,
} MotorCommandMailboxWriteKind;

typedef enum {
    MOTOR_COMMAND_MAILBOX_WRITE_QUEUE,
    MOTOR_COMMAND_MAILBOX_WRITE_WAIT,
    MOTOR_COMMAND_MAILBOX_WRITE_REPORT_FAILURE,
} MotorCommandMailboxWritePhase;

typedef enum {
    MOTOR_COMMAND_MAILBOX_WRITE_NONE,
    MOTOR_COMMAND_MAILBOX_WRITE_COMPLETE,
    MOTOR_COMMAND_MAILBOX_WRITE_FAILED,
} MotorCommandMailboxWriteResult;

typedef struct {
    MotorCommandMailboxWriteKind kind;
    MotorCommandMailboxWritePhase phase;
} MotorCommandMailboxWrite;

CommandTransportResult motor_command_mailbox_queue_payload(CommandTransport *transport,
                                                           const uint8_t *payload, uint16_t length);
CommandTransportResult motor_command_mailbox_queue_response(CommandTransport *transport,
                                                            uint8_t *response, uint16_t length);
CommandTransportResult
motor_command_mailbox_queue_status(CommandTransport *transport,
                                   const uint8_t status[MOTOR_COMMAND_MAILBOX_STATUS_SIZE]);
CommandTransportResult
motor_command_mailbox_queue_control(CommandTransport *transport,
                                    const uint8_t control[MOTOR_COMMAND_MAILBOX_REGISTER_SIZE]);
CommandTransportResult
motor_command_mailbox_queue_command(CommandTransport *transport,
                                    const uint8_t command[MOTOR_COMMAND_MAILBOX_REGISTER_SIZE]);
void motor_command_mailbox_write_init(MotorCommandMailboxWrite *write,
                                      MotorCommandMailboxWriteKind kind);
MotorCommandMailboxWriteResult motor_command_mailbox_write_run(MotorCommandMailboxWrite *write,
                                                               CommandTransport *transport,
                                                               const uint8_t *record,
                                                               uint32_t *accepted_value);

#endif
