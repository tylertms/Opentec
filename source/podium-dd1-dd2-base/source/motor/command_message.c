#include "motor/command_message.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal command identifiers and offsets in motor application payloads. */
enum {
    MOTOR_COMMAND_MESSAGE_INFORMATION_COMMAND = 0x85, /**< Information response command identifier. */
    MOTOR_COMMAND_MESSAGE_CALIBRATION_COMMAND = 0x87, /**< Calibration response command identifier. */
    MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION_COMMAND = 0xc1, /**< Vendor continuation command identifier. */
    MOTOR_COMMAND_MESSAGE_VENDOR_FINAL_COMMAND = 0xc2, /**< Vendor final command identifier. */
    MOTOR_COMMAND_MESSAGE_SELECTOR_OFFSET = 2, /**< Payload offset of the information selector. */
    MOTOR_COMMAND_MESSAGE_LENGTH_OFFSET = 4, /**< Payload offset of the data length. */
    MOTOR_COMMAND_MESSAGE_DATA_OFFSET = 5, /**< Payload offset of command data. */
};

bool motor_command_message_decode(const uint8_t *payload, uint16_t length,
                                  MotorCommandMessage *message) {
    if (payload == 0 || message == 0 || length == 0) {
        return false;
    }

    MotorCommandMessageKind kind;
    switch (payload[0]) {
    case MOTOR_COMMAND_MESSAGE_INFORMATION_COMMAND:
        kind = MOTOR_COMMAND_MESSAGE_INFORMATION;
        break;
    case MOTOR_COMMAND_MESSAGE_CALIBRATION_COMMAND:
        kind = MOTOR_COMMAND_MESSAGE_CALIBRATION;
        break;
    case MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION_COMMAND:
        *message = (MotorCommandMessage){
            .kind = MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION,
            .command = payload[0],
            .payload = payload,
            .payload_length = length,
            .data = payload,
            .data_length = length,
        };
        return true;
    case MOTOR_COMMAND_MESSAGE_VENDOR_FINAL_COMMAND:
        *message = (MotorCommandMessage){
            .kind = MOTOR_COMMAND_MESSAGE_VENDOR_FINAL,
            .command = payload[0],
            .payload = payload,
            .payload_length = length,
            .data = payload,
            .data_length = length,
        };
        return true;
    default:
        return false;
    }

    if (length < MOTOR_COMMAND_MESSAGE_DATA_OFFSET) {
        return false;
    }
    uint8_t data_length = payload[MOTOR_COMMAND_MESSAGE_LENGTH_OFFSET];
    if (data_length > length - MOTOR_COMMAND_MESSAGE_DATA_OFFSET) {
        return false;
    }
    *message = (MotorCommandMessage){
        .kind = kind,
        .command = payload[0],
        .selector = payload[MOTOR_COMMAND_MESSAGE_SELECTOR_OFFSET],
        .payload = payload,
        .payload_length = length,
        .data = payload + MOTOR_COMMAND_MESSAGE_DATA_OFFSET,
        .data_length = data_length,
    };
    return true;
}
