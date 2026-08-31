#include "motor/command_mailbox.h"

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Internal remote offsets used by the motor-command mailbox. */
enum {
    MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET = 0x80, /**< Remote mailbox payload record offset. */
    MOTOR_COMMAND_MAILBOX_LENGTH_OFFSET = 0x81, /**< Remote mailbox length record offset. */
    MOTOR_COMMAND_MAILBOX_CONTROL_OFFSET = 0x82, /**< Remote mailbox control record offset. */
    MOTOR_COMMAND_MAILBOX_STATUS_OFFSET = 0x90, /**< Remote mailbox status record offset. */
};

/**
 * @brief Prepares the shared transport for a mailbox operation.
 *
 * Polls and consumes any latched completion result, reporting busy only while the transport is
 * active or held by another owner.
 *
 * @param[in,out] transport Shared command transport to inspect.
 * @return Complete when another request can be queued, or busy while unavailable.
 */
static CommandTransportResult prepare_transport(CommandTransport *transport) {
    return command_transport_poll(transport, MOTOR_COMMAND_MAILBOX_OWNER) == COMMAND_TRANSPORT_BUSY
               ? COMMAND_TRANSPORT_BUSY
               : COMMAND_TRANSPORT_COMPLETE;
}

CommandTransportResult motor_command_mailbox_queue_payload_read(CommandTransport *transport,
                                                                uint8_t *payload, uint16_t length) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_read(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                        MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET, payload, length);
}

CommandTransportResult motor_command_mailbox_queue_payload_write(CommandTransport *transport,
                                                                 const uint8_t *payload,
                                                                 uint16_t length) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_write(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                         MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET, payload, length);
}

CommandTransportResult
motor_command_mailbox_queue_length_read(CommandTransport *transport,
                                        uint8_t length[MOTOR_COMMAND_MAILBOX_LENGTH_SIZE]) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_read(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                        MOTOR_COMMAND_MAILBOX_LENGTH_OFFSET, length,
                                        MOTOR_COMMAND_MAILBOX_LENGTH_SIZE);
}

CommandTransportResult
motor_command_mailbox_queue_control_read(CommandTransport *transport,
                                         uint8_t control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE]) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_read(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                        MOTOR_COMMAND_MAILBOX_CONTROL_OFFSET, control,
                                        MOTOR_COMMAND_MAILBOX_CONTROL_SIZE);
}

CommandTransportResult
motor_command_mailbox_queue_status_read(CommandTransport *transport,
                                        uint8_t status[MOTOR_COMMAND_MAILBOX_STATUS_SIZE]) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_read(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                        MOTOR_COMMAND_MAILBOX_STATUS_OFFSET, status,
                                        MOTOR_COMMAND_MAILBOX_STATUS_SIZE);
}

bool motor_command_mailbox_control_decode(const uint8_t record[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE],
                                          MotorCommandMailboxControl *control) {
    if (record == 0 || control == 0) {
        return false;
    }
    control->flags = record[0];
    control->reserved = record[1];
    control->payload_length = (uint16_t)((uint16_t)record[2] << 8) | record[3];
    return true;
}

bool motor_command_mailbox_exchange_init(MotorCommandMailboxExchange *exchange,
                                         uint8_t *read_buffer, uint16_t read_capacity) {
    if (exchange == 0 || read_buffer == 0 || read_capacity == 0) {
        return false;
    }
    *exchange = (MotorCommandMailboxExchange){
        .read_buffer = read_buffer,
        .read_capacity = read_capacity,
    };
    return true;
}

void motor_command_mailbox_exchange_reset(MotorCommandMailboxExchange *exchange) {
    uint8_t *read_buffer = exchange->read_buffer;
    uint16_t read_capacity = exchange->read_capacity;
    *exchange = (MotorCommandMailboxExchange){
        .read_buffer = read_buffer,
        .read_capacity = read_capacity,
    };
}

bool motor_command_mailbox_exchange_queue(MotorCommandMailboxExchange *exchange,
                                          const uint8_t *packet, uint16_t length) {
    if (exchange == 0 || packet == 0 || length == 0 || length > MEMORY_TRANSFER_MAX_WRITE_SIZE ||
        (exchange->write_packet != 0 &&
         (exchange->write_packet != packet ||
          exchange->phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_WRITE_WAIT))) {
        return false;
    }
    exchange->write_packet = packet;
    exchange->write_length = length;
    return true;
}

MotorCommandMailboxExchangeResult
motor_command_mailbox_exchange_run(MotorCommandMailboxExchange *exchange,
                                   CommandTransport *transport) {
    MotorCommandMailboxExchangeResult output = {0};
    if (exchange == 0 || transport == 0) {
        output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        return output;
    }

    if (exchange->phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE) {
        CommandTransportResult result =
            motor_command_mailbox_queue_control_read(transport, exchange->control_record);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_WAIT;
        }
        return output;
    }

    CommandTransportResult completion =
        command_transport_poll(transport, MOTOR_COMMAND_MAILBOX_OWNER);
    if (completion == COMMAND_TRANSPORT_BUSY) {
        return output;
    }
    if (completion != COMMAND_TRANSPORT_COMPLETE) {
        exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
        output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        return output;
    }

    if (exchange->phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_READ_WAIT) {
        exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
        output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ;
        output.packet = exchange->read_buffer;
        output.packet_length = exchange->read_length;
        return output;
    }
    if (exchange->phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_WRITE_WAIT) {
        exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
        exchange->write_packet = 0;
        exchange->write_length = 0;
        output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN;
        return output;
    }
    if (exchange->phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_WAIT) {
        exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
        output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_READ;
        output.status = (uint32_t)exchange->status_record[0] << 24 |
                        (uint32_t)exchange->status_record[1] << 16 |
                        (uint32_t)exchange->status_record[2] << 8 | exchange->status_record[3];
        return output;
    }

    if (!motor_command_mailbox_control_decode(exchange->control_record, &exchange->control)) {
        exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
        output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        return output;
    }
    if ((exchange->control.flags & MOTOR_COMMAND_MAILBOX_CONTROL_STATUS_MASK) ==
        MOTOR_COMMAND_MAILBOX_CONTROL_STATUS_RETRY) {
        exchange->status_retry_count++;
        if (exchange->status_retry_count > 1 &&
            motor_command_mailbox_queue_status_read(transport, exchange->status_record) ==
                COMMAND_TRANSPORT_COMPLETE) {
            exchange->status_retry_count = 0;
            exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_WAIT;
        } else {
            exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
        }
        return output;
    }
    exchange->status_retry_count = 0;

    if ((exchange->control.flags & MOTOR_COMMAND_MAILBOX_CONTROL_PAYLOAD_AVAILABLE) != 0) {
        if (exchange->control.payload_length > exchange->read_capacity ||
            motor_command_mailbox_queue_payload_read(transport, exchange->read_buffer,
                                                     exchange->control.payload_length) !=
                COMMAND_TRANSPORT_COMPLETE) {
            exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
            output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
            return output;
        }
        exchange->read_length = exchange->control.payload_length;
        exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_READ_WAIT;
        return output;
    }

    if (exchange->write_packet != 0) {
        if (motor_command_mailbox_queue_payload_write(transport, exchange->write_packet,
                                                      exchange->write_length) !=
            COMMAND_TRANSPORT_COMPLETE) {
            exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
            output.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
            return output;
        }
        exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_WRITE_WAIT;
        return output;
    }

    exchange->phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
    return output;
}
