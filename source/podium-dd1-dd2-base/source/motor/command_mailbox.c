#include "motor/command_mailbox.h"

#include <stdint.h>

#include "transfer/command.h"

enum {
    MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET = 0x80,
    MOTOR_COMMAND_MAILBOX_CONTROL_OFFSET = 0x82,
    MOTOR_COMMAND_MAILBOX_COMMAND_OFFSET = 0x90,
};

/**
 * @brief Queues a motor-command mailbox payload write.
 *
 * Writes the encoded packet bytes through command-transport owner 0x20 at remote mailbox offset
 * 0x80.
 *
 * @param[in,out] transport Shared command transport receiving the write request.
 * @param[in] payload Encoded motor-command packet bytes.
 * @param[in] length Encoded packet byte count.
 * @return Command-transport queue result.
 */
CommandTransportResult motor_command_mailbox_queue_payload(CommandTransport *transport,
                                                           const uint8_t *payload,
                                                           uint16_t length) {
    return command_transport_queue_write(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                         MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET, payload, length);
}

/**
 * @brief Queues a motor-command mailbox control write.
 *
 * Writes the four-byte control record through command-transport owner 0x20 at remote mailbox
 * offset 0x82.
 *
 * @param[in,out] transport Shared command transport receiving the write request.
 * @param[in] control Four-byte motor-command control record.
 * @return Command-transport queue result.
 */
CommandTransportResult
motor_command_mailbox_queue_control(CommandTransport *transport,
                                    const uint8_t control[MOTOR_COMMAND_MAILBOX_REGISTER_SIZE]) {
    return command_transport_queue_write(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                         MOTOR_COMMAND_MAILBOX_CONTROL_OFFSET, control,
                                         MOTOR_COMMAND_MAILBOX_REGISTER_SIZE);
}

/**
 * @brief Queues a motor-command mailbox command write.
 *
 * Writes the four-byte command record through command-transport owner 0x20 at remote mailbox
 * offset 0x90.
 *
 * @param[in,out] transport Shared command transport receiving the write request.
 * @param[in] command Four-byte motor-command command record.
 * @return Command-transport queue result.
 */
CommandTransportResult
motor_command_mailbox_queue_command(CommandTransport *transport,
                                    const uint8_t command[MOTOR_COMMAND_MAILBOX_REGISTER_SIZE]) {
    return command_transport_queue_write(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                         MOTOR_COMMAND_MAILBOX_COMMAND_OFFSET, command,
                                         MOTOR_COMMAND_MAILBOX_REGISTER_SIZE);
}
