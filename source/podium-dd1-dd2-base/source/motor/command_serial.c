#include "motor/command_serial.h"

#include <stdbool.h>
#include <stdint.h>

#include "serial/message.h"
#include "serial/service.h"
#include "transfer/command.h"

enum {
    MOTOR_COMMAND_SERIAL_MESSAGE_TYPE = 4,
};

/**
 * @brief Submits a queued command through the shared serial session.
 *
 * Queues the complete command request as a type-four logical message and advances the command
 * transport to its response-wait phase after the serial session accepts it.
 *
 * @param[in,out] transport Command transport with a queued request.
 * @param[in,out] service Shared serial service accepting the request.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the request was queued for serial transmission.
 */
bool motor_command_serial_submit(CommandTransport *transport, SerialService *service,
                                 uint32_t now_ms) {
    if (transport == 0 || service == 0) {
        return false;
    }
    if ((transport->phase != COMMAND_TRANSPORT_WRITE_QUEUED &&
         transport->phase != COMMAND_TRANSPORT_READ_QUEUED) ||
        !serial_service_start(service, MOTOR_COMMAND_SERIAL_MESSAGE_TYPE, transport->request,
                              transport->request_length, now_ms)) {
        return false;
    }
    return command_transport_request_sent(transport);
}

/**
 * @brief Applies a type-four serial command response.
 *
 * Routes a completed type-four logical message to the command transport, consumes the incoming
 * message, and releases the corresponding outgoing serial request.
 *
 * @param[in,out] transport Command transport awaiting a response.
 * @param[in,out] service Shared serial service holding the completed message.
 * @return True when a type-four response was applied.
 */
bool motor_command_serial_receive(CommandTransport *transport, SerialService *service) {
    if (transport == 0 || service == 0 ||
        service->request_type != MOTOR_COMMAND_SERIAL_MESSAGE_TYPE) {
        return false;
    }
    if (service->status == SERIAL_SERVICE_FAILED) {
        command_transport_fail(transport);
        serial_service_release(service);
        return true;
    }
    const SerialMessageAssembly *message = serial_service_response(service);
    if (message == 0) {
        return false;
    }
    command_transport_receive(transport, message->data, message->length);
    serial_service_release(service);
    return true;
}
