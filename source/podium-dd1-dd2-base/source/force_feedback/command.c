#include "force_feedback/command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    FORCE_FEEDBACK_COMMAND_SIZE = 7,
    FORCE_FEEDBACK_CONFIGURE_OPCODE = 1,
    FORCE_FEEDBACK_CLEAR_OPCODE = 3,
    FORCE_FEEDBACK_ACTIVATE_POSITION_OPCODE = 4,
    FORCE_FEEDBACK_CLEAR_POSITION_OPCODE = 5,
    FORCE_FEEDBACK_PRIMARY_OUTPUT_PREFIX = 0xfa,
    FORCE_FEEDBACK_SECONDARY_OUTPUT_PREFIX = 0xfb,
    FORCE_FEEDBACK_KIND_1 = 8,
    FORCE_FEEDBACK_KIND_2 = 0x0b,
    FORCE_FEEDBACK_KIND_3 = 0x0c,
};

/**
 * @brief Decodes a force-feedback command from the short HID output report.
 *
 * Classifies slot configuration, slot clearing, position-effect control, and the primary or
 * secondary output gates. Configuration payloads are expanded into signed directions, 16-bit
 * strengths, and the signed kind-1 magnitude used by the effect engine.
 *
 * @param[in] output Classified short HID output payload.
 * @param[out] command Destination for the decoded force-feedback command.
 * @return True when the seven-byte payload is a supported force-feedback command.
 */
bool force_feedback_command_decode(const UsbOutputCommand *output, ForceFeedbackCommand *command) {
    if (output == NULL || command == NULL || output->kind != USB_OUTPUT_COMMAND_SHORT ||
        output->payload == NULL || output->length != FORCE_FEEDBACK_COMMAND_SIZE) {
        return false;
    }

    const uint8_t *payload = output->payload;
    uint8_t opcode = payload[0] & 0x0f;
    uint8_t slot = payload[0] >> 4;
    *command = (ForceFeedbackCommand){.slot = slot};

    if (opcode == FORCE_FEEDBACK_CONFIGURE_OPCODE) {
        if (payload[1] == FORCE_FEEDBACK_KIND_1) {
            command->kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1;
            if (payload[6] == 0 && payload[2] != 0x80) {
                command->magnitude = ((int32_t)0x8000 - payload[2] * 0x101L) * 2 - 1;
            } else if (payload[6] == 1) {
                uint16_t input = payload[2] | (uint16_t)payload[3] << 8;
                command->magnitude = ((int32_t)0x8000 - input) * 2 - 1;
            }
            return true;
        }

        if (payload[1] == FORCE_FEEDBACK_KIND_2) {
            command->kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2;
            command->positions[0] = payload[2];
            command->positions[1] = payload[3];
            command->axis_modes[0] = payload[4] & 0x0f;
            command->axis_modes[1] = payload[4] >> 4;
            command->directions[0] = (payload[5] & 1) != 0 ? 1 : -1;
            command->directions[1] = (payload[5] & 0x10) != 0 ? 1 : -1;
            command->strength = payload[6] * 0x101U;
            return true;
        }

        if (payload[1] == FORCE_FEEDBACK_KIND_3) {
            command->kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3;
            command->mode = payload[2] & 0x0f;
            command->directions[0] = (payload[3] & 1) != 0 ? -1 : 1;
            command->axis_modes[0] = payload[4] & 0x0f;
            command->directions[1] = (payload[5] & 1) != 0 ? -1 : 1;
            command->strength = payload[6] * 0x101U;
            return true;
        }

        return false;
    }

    if (opcode == FORCE_FEEDBACK_CLEAR_OPCODE) {
        command->kind = FORCE_FEEDBACK_COMMAND_CLEAR_EFFECT;
        return true;
    }

    if (payload[0] >= 0x10 && opcode == FORCE_FEEDBACK_ACTIVATE_POSITION_OPCODE) {
        command->kind = FORCE_FEEDBACK_COMMAND_ACTIVATE_POSITION_EFFECT;
        command->slot = FORCE_FEEDBACK_POSITION_EFFECT_SLOT;
        return true;
    }

    if (payload[0] >= 0x10 && opcode == FORCE_FEEDBACK_CLEAR_POSITION_OPCODE) {
        command->kind = FORCE_FEEDBACK_COMMAND_CLEAR_POSITION_EFFECT;
        command->slot = FORCE_FEEDBACK_POSITION_EFFECT_SLOT;
        return true;
    }

    if (payload[0] == FORCE_FEEDBACK_PRIMARY_OUTPUT_PREFIX) {
        command->kind = FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT;
        command->output_disabled = payload[1] == 0xc7;
        return true;
    }

    if (payload[0] == FORCE_FEEDBACK_SECONDARY_OUTPUT_PREFIX) {
        command->kind = FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT;
        command->output_disabled = payload[1] == 1;
        return true;
    }

    return false;
}
