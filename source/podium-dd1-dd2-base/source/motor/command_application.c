#include "motor/command_application.h"

#include <stdint.h>
#include <string.h>

#include "motor/command_digest.h"
#include "motor/command_information.h"
#include "motor/command_message.h"

/** @brief Internal application-response sizes. */
enum {
    MOTOR_COMMAND_APPLICATION_CALIBRATION_SIZE = 20, /**< Required calibration response data length. */
};

void motor_command_application_init(MotorCommandApplication *application) {
    memset(application, 0, sizeof(*application));
}

MotorCommandApplicationEvent motor_command_application_apply(MotorCommandApplication *application,
                                                             const MotorCommandMessage *message) {
    MotorCommandApplicationEvent event = {.result = MOTOR_COMMAND_APPLICATION_INVALID};
    if (application == 0 || message == 0) {
        return event;
    }

    if (message->kind == MOTOR_COMMAND_MESSAGE_INFORMATION) {
        MotorCommandInformationResult result =
            motor_command_information_apply(&application->information, message);
        if (result == MOTOR_COMMAND_INFORMATION_STORED) {
            event.result = MOTOR_COMMAND_APPLICATION_INFORMATION;
        } else if (result == MOTOR_COMMAND_INFORMATION_FORWARD) {
            event.result = MOTOR_COMMAND_APPLICATION_FORWARD;
            event.forward_data = message->payload;
            event.forward_length = message->payload_length;
        }
        return event;
    }

    if (message->kind == MOTOR_COMMAND_MESSAGE_CALIBRATION) {
        if (message->data == 0 ||
            message->data_length != MOTOR_COMMAND_APPLICATION_CALIBRATION_SIZE) {
            return event;
        }
        motor_command_digest_encode(message->data, application->digest);
        event.result = MOTOR_COMMAND_APPLICATION_CALIBRATION;
        return event;
    }

    if (message->kind == MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION ||
        message->kind == MOTOR_COMMAND_MESSAGE_VENDOR_FINAL) {
        if (message->data == 0 || message->data_length == 0) {
            return event;
        }
        event.result = MOTOR_COMMAND_APPLICATION_FORWARD;
        event.forward_data = message->payload;
        event.forward_length = message->payload_length;
    }
    return event;
}
