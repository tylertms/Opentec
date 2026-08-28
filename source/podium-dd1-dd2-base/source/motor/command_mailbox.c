#include "motor/command_mailbox.h"

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

enum {
    MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET = 0x80,
    MOTOR_COMMAND_MAILBOX_LENGTH_OFFSET = 0x81,
    MOTOR_COMMAND_MAILBOX_CONTROL_OFFSET = 0x82,
    MOTOR_COMMAND_MAILBOX_STATUS_OFFSET = 0x90,
};

/**
 * @brief Prepares the shared transport for a mailbox operation.
 *
 * Consumes a completed or rejected result and reports busy only while the transport is active or
 * held by another owner.
 *
 * @param[in,out] transport Shared command transport to inspect.
 * @return Complete when another request can be queued, or busy while unavailable.
 */
static CommandTransportResult prepare_transport(CommandTransport *transport) {
    return command_transport_poll(transport, MOTOR_COMMAND_MAILBOX_OWNER) == COMMAND_TRANSPORT_BUSY
               ? COMMAND_TRANSPORT_BUSY
               : COMMAND_TRANSPORT_COMPLETE;
}

/**
 * @brief Queues a mailbox packet read.
 *
 * Reads the selected number of packet bytes from remote offset 0x80 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the request.
 * @param[out] payload Destination for the returned packet bytes.
 * @param[in] length Requested packet byte count.
 * @return Command-transport queue result.
 */
CommandTransportResult motor_command_mailbox_queue_payload_read(CommandTransport *transport,
                                                                uint8_t *payload, uint16_t length) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_read(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                        MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET, payload, length);
}

/**
 * @brief Queues a mailbox packet write.
 *
 * Writes the selected packet bytes to remote offset 0x80 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the request.
 * @param[in] payload Packet bytes to write.
 * @param[in] length Packet byte count.
 * @return Command-transport queue result.
 */
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

/**
 * @brief Queues a mailbox capacity read.
 *
 * Reads the two-byte packet capacity from remote offset 0x81 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the request.
 * @param[out] length Two-byte capacity record.
 * @return Command-transport queue result.
 */
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

/**
 * @brief Queues a mailbox control read.
 *
 * Reads the four-byte availability and packet-length record from remote offset 0x82 through owner
 * 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the request.
 * @param[out] control Four-byte control record.
 * @return Command-transport queue result.
 */
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

/**
 * @brief Queues a mailbox status read.
 *
 * Reads the four-byte status record from remote offset 0x90 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the request.
 * @param[out] status Four-byte status record.
 * @return Command-transport queue result.
 */
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

/**
 * @brief Decodes a mailbox control record.
 *
 * Separates the availability flags and reserved byte and combines the packet length in big-endian
 * order.
 *
 * @param[in] record Four-byte control record.
 * @param[out] control Decoded mailbox control values.
 * @return True when both arguments are present.
 */
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

/**
 * @brief Initializes a motor-command mailbox exchange.
 *
 * Attaches caller-owned packet storage and starts the exchange by polling the remote control
 * record.
 *
 * @param[out] exchange Mailbox exchange to initialize.
 * @param[out] read_buffer Storage for packets read from the remote mailbox.
 * @param[in] read_capacity Available packet storage in bytes.
 * @return True when the exchange and packet storage are usable.
 */
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

/**
 * @brief Resets a motor-command mailbox exchange.
 *
 * Discards pending transfers and retry state while retaining the caller-owned receive storage.
 *
 * @param[in,out] exchange Mailbox exchange to reset.
 */
void motor_command_mailbox_exchange_reset(MotorCommandMailboxExchange *exchange) {
    uint8_t *read_buffer = exchange->read_buffer;
    uint16_t read_capacity = exchange->read_capacity;
    *exchange = (MotorCommandMailboxExchange){
        .read_buffer = read_buffer,
        .read_capacity = read_capacity,
    };
}

/**
 * @brief Queues a packet for the remote motor-command mailbox.
 *
 * Retains the caller-owned packet until the exchange reports that the payload write completed. A
 * replacement using the same storage can update the packet until its payload write starts.
 *
 * @param[in,out] exchange Mailbox exchange receiving the packet.
 * @param[in] packet Packet bytes to write.
 * @param[in] length Packet byte count.
 * @return True when the packet was queued or replaced and fits one command transfer.
 */
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

/**
 * @brief Advances a motor-command mailbox exchange.
 *
 * Polls control before every transfer, gives incoming packets priority, reads status after two
 * consecutive retry indications, and writes a queued packet only while the remote mailbox has no
 * packet waiting.
 *
 * @param[in,out] exchange Mailbox exchange state to advance.
 * @param[in,out] transport Shared owner-0x20 command transport.
 * @return Packet, status, completion, or failure event produced by this service call.
 */
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
