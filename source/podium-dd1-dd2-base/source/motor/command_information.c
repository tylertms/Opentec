#include "motor/command_information.h"

#include <stdint.h>
#include <string.h>

#include "motor/command_message.h"

/** @brief Internal information selector identifiers. */
enum {
    MOTOR_COMMAND_INFORMATION_SELECTOR_1 = 1, /**< Information selector 1. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_2 = 2, /**< Information selector 2. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_3 = 3, /**< Information selector 3. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_4 = 4, /**< Information selector 4. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_5 = 5, /**< Information selector 5. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_6 = 6, /**< Information selector 6. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_7 = 7, /**< Information selector 7. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_8 = 8, /**< Information selector 8. */
    MOTOR_COMMAND_INFORMATION_SELECTOR_9 = 9, /**< Information selector 9. */
};

/**
 * @brief Decodes a high-byte-first information word.
 *
 * Combines the two response bytes into their logical 16-bit value.
 *
 * @param[in] data Two-byte information field.
 * @return Decoded 16-bit value.
 */
static uint16_t decode_word(const uint8_t data[2]) { return ((uint16_t)data[0] << 8) | data[1]; }

MotorCommandInformationResult motor_command_information_apply(MotorCommandInformation *state,
                                                              const MotorCommandMessage *message) {
    if (state == 0 || message == 0 || message->kind != MOTOR_COMMAND_MESSAGE_INFORMATION ||
        message->data == 0) {
        return MOTOR_COMMAND_INFORMATION_INVALID;
    }

    switch (message->selector) {
    case MOTOR_COMMAND_INFORMATION_SELECTOR_1:
        if (message->data_length != 2) {
            return MOTOR_COMMAND_INFORMATION_INVALID;
        }
        state->selector_1 = decode_word(message->data);
        break;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_2:
        return message->data_length == 20 ? MOTOR_COMMAND_INFORMATION_FORWARD
                                          : MOTOR_COMMAND_INFORMATION_INVALID;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_3:
        if (message->data_length != 2) {
            return MOTOR_COMMAND_INFORMATION_INVALID;
        }
        state->selector_3 = decode_word(message->data);
        break;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_4:
        if (message->data_length != 2) {
            return MOTOR_COMMAND_INFORMATION_INVALID;
        }
        state->selector_4 = decode_word(message->data);
        break;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_5:
        if (message->data_length != 1) {
            return MOTOR_COMMAND_INFORMATION_INVALID;
        }
        state->selector_5 = message->data[0];
        break;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_6:
        return message->data_length == 2 ? MOTOR_COMMAND_INFORMATION_STORED
                                         : MOTOR_COMMAND_INFORMATION_INVALID;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_7:
        if (message->data_length != sizeof(state->selector_7)) {
            return MOTOR_COMMAND_INFORMATION_INVALID;
        }
        memcpy(state->selector_7, message->data, sizeof(state->selector_7));
        break;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_8:
        if (message->data_length != sizeof(state->selector_8)) {
            return MOTOR_COMMAND_INFORMATION_INVALID;
        }
        memcpy(state->selector_8, message->data, sizeof(state->selector_8));
        break;
    case MOTOR_COMMAND_INFORMATION_SELECTOR_9:
        if (message->data_length != sizeof(state->selector_9)) {
            return MOTOR_COMMAND_INFORMATION_INVALID;
        }
        memcpy(state->selector_9, message->data, sizeof(state->selector_9));
        break;
    default:
        return MOTOR_COMMAND_INFORMATION_INVALID;
    }
    return MOTOR_COMMAND_INFORMATION_STORED;
}
