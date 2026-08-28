#include "motor/command_mailbox.h"

#include <stdint.h>

#include "transfer/command.h"

enum {
    MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET = 0x80,
    MOTOR_COMMAND_MAILBOX_STATUS_OFFSET = 0x81,
    MOTOR_COMMAND_MAILBOX_CONTROL_OFFSET = 0x82,
    MOTOR_COMMAND_MAILBOX_COMMAND_OFFSET = 0x90,
};

/**
 * @brief Prepares the motor-command mailbox transport for another request.
 *
 * Consumes any completed or rejected request result and reports busy only while the shared
 * transport still has an active operation or a different owner.
 *
 * @param[in,out] transport Shared command transport to inspect.
 * @return Complete when another mailbox request can be queued, or busy while unavailable.
 */
static CommandTransportResult prepare_transport(CommandTransport *transport) {
    return command_transport_poll(transport, MOTOR_COMMAND_MAILBOX_OWNER) == COMMAND_TRANSPORT_BUSY
               ? COMMAND_TRANSPORT_BUSY
               : COMMAND_TRANSPORT_COMPLETE;
}

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
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_write(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                         MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET, payload, length);
}

/**
 * @brief Queues a motor-command mailbox response read.
 *
 * Reads the requested response bytes through command-transport owner 0x20 at remote mailbox
 * offset 0x80. The shared transport applies its per-transfer limit.
 *
 * @param[in,out] transport Shared command transport receiving the read request.
 * @param[out] response Destination for the response bytes.
 * @param[in] length Requested response byte count.
 * @return Command-transport queue result.
 */
CommandTransportResult motor_command_mailbox_queue_response(CommandTransport *transport,
                                                            uint8_t *response, uint16_t length) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_read(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                        MOTOR_COMMAND_MAILBOX_PAYLOAD_OFFSET, response, length);
}

/**
 * @brief Queues a motor-command mailbox status write.
 *
 * Writes the two-byte status record through command-transport owner 0x20 at remote mailbox offset
 * 0x81.
 *
 * @param[in,out] transport Shared command transport receiving the write request.
 * @param[in] status Two-byte motor-command status record.
 * @return Command-transport queue result.
 */
CommandTransportResult
motor_command_mailbox_queue_status(CommandTransport *transport,
                                   const uint8_t status[MOTOR_COMMAND_MAILBOX_STATUS_SIZE]) {
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_write(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                         MOTOR_COMMAND_MAILBOX_STATUS_OFFSET, status,
                                         MOTOR_COMMAND_MAILBOX_STATUS_SIZE);
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
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
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
    CommandTransportResult result = prepare_transport(transport);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return result;
    }
    return command_transport_queue_write(transport, MOTOR_COMMAND_MAILBOX_OWNER,
                                         MOTOR_COMMAND_MAILBOX_COMMAND_OFFSET, command,
                                         MOTOR_COMMAND_MAILBOX_REGISTER_SIZE);
}

/**
 * @brief Initializes a motor-command mailbox register write.
 *
 * Selects the status, control, or command register and starts the staged writer in its queue
 * phase.
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
 * Queues the selected register, waits for command-transport completion, and reports a rejected
 * write on the following service call. Successful status- and command-register writes publish the
 * register bytes as one big-endian value.
 *
 * @param[in,out] write Mailbox register write state to advance.
 * @param[in,out] transport Shared command transport used by the write.
 * @param[in] record Selected status, control, or command register bytes.
 * @param[out] accepted_value Big-endian status- or command-register value, or null when unused.
 * @return No event while queued, complete after acceptance, or failed after rejection.
 */
MotorCommandMailboxWriteResult motor_command_mailbox_write_run(MotorCommandMailboxWrite *write,
                                                               CommandTransport *transport,
                                                               const uint8_t *record,
                                                               uint32_t *accepted_value) {
    if (write->phase == MOTOR_COMMAND_MAILBOX_WRITE_REPORT_FAILURE) {
        write->phase = MOTOR_COMMAND_MAILBOX_WRITE_QUEUE;
        return MOTOR_COMMAND_MAILBOX_WRITE_FAILED;
    }
    if (write->phase == MOTOR_COMMAND_MAILBOX_WRITE_QUEUE) {
        CommandTransportResult result;
        if (write->kind == MOTOR_COMMAND_MAILBOX_STATUS_WRITE) {
            result = motor_command_mailbox_queue_status(transport, record);
        } else if (write->kind == MOTOR_COMMAND_MAILBOX_COMMAND_WRITE) {
            result = motor_command_mailbox_queue_command(transport, record);
        } else {
            result = motor_command_mailbox_queue_control(transport, record);
        }
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
    if (accepted_value != 0) {
        if (write->kind == MOTOR_COMMAND_MAILBOX_STATUS_WRITE) {
            *accepted_value = (uint16_t)((uint16_t)record[0] << 8) | record[1];
        } else if (write->kind == MOTOR_COMMAND_MAILBOX_COMMAND_WRITE) {
            *accepted_value = (uint32_t)record[0] << 24 | (uint32_t)record[1] << 16 |
                              (uint32_t)record[2] << 8 | record[3];
        }
    }
    return MOTOR_COMMAND_MAILBOX_WRITE_COMPLETE;
}
