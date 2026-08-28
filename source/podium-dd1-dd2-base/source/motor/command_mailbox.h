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
    MOTOR_COMMAND_MAILBOX_CONTROL_PAYLOAD_AVAILABLE = 0x40,
    MOTOR_COMMAND_MAILBOX_CONTROL_STATUS_MASK = 0xc0,
    MOTOR_COMMAND_MAILBOX_CONTROL_STATUS_RETRY = 0x80,
};

typedef struct {
    uint16_t payload_length;
    uint8_t flags;
    uint8_t reserved;
} MotorCommandMailboxControl;

typedef enum {
    MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_WAIT,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_READ_WAIT,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_WRITE_WAIT,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_WAIT,
} MotorCommandMailboxExchangePhase;

typedef enum {
    MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_READ,
    MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED,
} MotorCommandMailboxExchangeEvent;

typedef struct {
    MotorCommandMailboxExchangeEvent event;
    const uint8_t *packet;
    uint16_t packet_length;
    uint32_t status;
} MotorCommandMailboxExchangeResult;

typedef struct {
    uint8_t control_record[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE];
    uint8_t status_record[MOTOR_COMMAND_MAILBOX_STATUS_SIZE];
    MotorCommandMailboxControl control;
    uint8_t *read_buffer;
    const uint8_t *write_packet;
    uint16_t read_capacity;
    uint16_t read_length;
    uint16_t write_length;
    MotorCommandMailboxExchangePhase phase;
    uint8_t status_retry_count;
} MotorCommandMailboxExchange;

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
bool motor_command_mailbox_exchange_init(MotorCommandMailboxExchange *exchange,
                                         uint8_t *read_buffer, uint16_t read_capacity);
bool motor_command_mailbox_exchange_queue(MotorCommandMailboxExchange *exchange,
                                          const uint8_t *packet, uint16_t length);
MotorCommandMailboxExchangeResult
motor_command_mailbox_exchange_run(MotorCommandMailboxExchange *exchange,
                                   CommandTransport *transport);

#endif
