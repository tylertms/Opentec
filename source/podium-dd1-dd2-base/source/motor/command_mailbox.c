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
