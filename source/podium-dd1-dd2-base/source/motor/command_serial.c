#include "motor/command_serial.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor/serial_message.h"
#include "motor/serial_session.h"
#include "transfer/command.h"

enum {
    MOTOR_COMMAND_SERIAL_MESSAGE_TYPE = 4,
};

/**
 * @brief Submits a queued command through the motor serial session.
 *
 * Queues the complete command request as a type-four logical message and advances the command
 * transport to its response-wait phase after the serial session accepts it.
 *
 * @param[in,out] transport Command transport with a queued request.
 * @param[in,out] session Motor serial session accepting the request.
 * @return True when the request was queued for serial transmission.
 */
bool motor_command_serial_submit(CommandTransport *transport, MotorSerialSession *session) {
    if (transport == 0 || session == 0) {
        return false;
    }
    if ((transport->phase != COMMAND_TRANSPORT_WRITE_QUEUED &&
         transport->phase != COMMAND_TRANSPORT_READ_QUEUED) ||
        !motor_serial_session_queue(session, MOTOR_COMMAND_SERIAL_MESSAGE_TYPE, transport->request,
                                    transport->request_length)) {
        return false;
    }
    return command_transport_request_sent(transport);
}

/**
 * @brief Applies a motor serial command response.
 *
 * Routes a completed type-four logical message to the command transport, consumes the incoming
 * message, and releases the corresponding outgoing serial request.
 *
 * @param[in,out] transport Command transport awaiting a response.
 * @param[in,out] session Motor serial session holding the completed message.
 * @return True when a type-four response was applied.
 */
bool motor_command_serial_receive(CommandTransport *transport, MotorSerialSession *session) {
    if (transport == 0 || session == 0) {
        return false;
    }
    const MotorSerialMessageAssembly *message = motor_serial_session_message(session);
    if (message == 0 || message->type != MOTOR_COMMAND_SERIAL_MESSAGE_TYPE) {
        return false;
    }
    command_transport_receive(transport, message->data, message->length);
    motor_serial_session_consume_message(session);
    motor_serial_session_finish_transmit(session);
    return true;
}
