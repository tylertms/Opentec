#include "motor/command_message.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    MOTOR_COMMAND_MESSAGE_INFORMATION_COMMAND = 0x85,
    MOTOR_COMMAND_MESSAGE_CALIBRATION_COMMAND = 0x87,
    MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION_COMMAND = 0xc1,
    MOTOR_COMMAND_MESSAGE_VENDOR_FINAL_COMMAND = 0xc2,
    MOTOR_COMMAND_MESSAGE_SELECTOR_OFFSET = 2,
    MOTOR_COMMAND_MESSAGE_LENGTH_OFFSET = 4,
    MOTOR_COMMAND_MESSAGE_DATA_OFFSET = 5,
};

/**
 * @brief Decodes a complete motor-command application message.
 *
 * Classifies information, calibration, and vendor-transfer responses. Information and calibration
 * messages expose their selector and bounded data field, while vendor responses expose the whole
 * application payload for the vendor service.
 *
 * @param[in] payload Complete application payload after transport reassembly.
 * @param[in] length Application payload byte count.
 * @param[out] message Decoded command view.
 * @return True when the command and its required fields are supported and complete.
 */
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
