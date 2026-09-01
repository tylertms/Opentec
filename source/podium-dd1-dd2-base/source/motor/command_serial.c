#include "motor/command_serial.h"

#include <stdbool.h>
#include <stdint.h>

#include "serial/message.h"
#include "serial/service.h"
#include "transfer/command.h"

/**
 * @brief Logical serial message type used for command transport requests.
 */
enum {
    MOTOR_COMMAND_SERIAL_MESSAGE_TYPE = 4, /**< Type assigned to command transport messages. */
};

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
