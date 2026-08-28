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

/**
 * @brief Initializes a motor-command mailbox register write.
 *
 * Selects the control or command register and starts the staged writer in its queue phase.
 *
 * @param[out] write Mailbox write state to initialize.
 * @param[in] kind Control or command register selection.
 */
void motor_command_mailbox_write_init(MotorCommandMailboxWrite *write,
                                      MotorCommandMailboxWriteKind kind) {
    write->kind = kind;
    write->phase = MOTOR_COMMAND_MAILBOX_WRITE_QUEUE;
}

/**
 * @brief Advances a motor-command mailbox register write.
 *
 * Queues the selected four-byte register, waits for command-transport completion, and reports a
 * rejected write on the following service call. Successful command-register writes publish the
 * register bytes as one big-endian 32-bit value.
 *
 * @param[in,out] write Mailbox register write state to advance.
 * @param[in,out] transport Shared command transport used by the write.
 * @param[in] record Four-byte control or command register value.
 * @param[out] command_value Big-endian command-register value on successful command writes.
 * @return No event while queued, complete after acceptance, or failed after rejection.
 */
MotorCommandMailboxWriteResult
motor_command_mailbox_write_run(MotorCommandMailboxWrite *write, CommandTransport *transport,
                                const uint8_t record[MOTOR_COMMAND_MAILBOX_REGISTER_SIZE],
                                uint32_t *command_value) {
    if (write->phase == MOTOR_COMMAND_MAILBOX_WRITE_REPORT_FAILURE) {
        write->phase = MOTOR_COMMAND_MAILBOX_WRITE_QUEUE;
        return MOTOR_COMMAND_MAILBOX_WRITE_FAILED;
    }
    if (write->phase == MOTOR_COMMAND_MAILBOX_WRITE_QUEUE) {
        CommandTransportResult result =
            write->kind == MOTOR_COMMAND_MAILBOX_COMMAND_WRITE
                ? motor_command_mailbox_queue_command(transport, record)
                : motor_command_mailbox_queue_control(transport, record);
        write->phase = result == COMMAND_TRANSPORT_COMPLETE
                           ? MOTOR_COMMAND_MAILBOX_WRITE_WAIT
                           : MOTOR_COMMAND_MAILBOX_WRITE_REPORT_FAILURE;
        return MOTOR_COMMAND_MAILBOX_WRITE_NONE;
    }
    CommandTransportResult result = command_transport_poll(transport, MOTOR_COMMAND_MAILBOX_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return MOTOR_COMMAND_MAILBOX_WRITE_NONE;
    }
    if (result == COMMAND_TRANSPORT_WRITE_REJECTED || result == COMMAND_TRANSPORT_READ_REJECTED) {
        write->phase = MOTOR_COMMAND_MAILBOX_WRITE_REPORT_FAILURE;
        return MOTOR_COMMAND_MAILBOX_WRITE_NONE;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return MOTOR_COMMAND_MAILBOX_WRITE_NONE;
    }
    write->phase = MOTOR_COMMAND_MAILBOX_WRITE_QUEUE;
    if (write->kind == MOTOR_COMMAND_MAILBOX_COMMAND_WRITE && command_value != 0) {
        *command_value = (uint32_t)record[0] << 24 | (uint32_t)record[1] << 16 |
                         (uint32_t)record[2] << 8 | record[3];
    }
    return MOTOR_COMMAND_MAILBOX_WRITE_COMPLETE;
}
