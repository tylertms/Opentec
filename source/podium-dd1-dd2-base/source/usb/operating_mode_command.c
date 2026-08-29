#include "usb/operating_mode_command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    OPERATING_MODE_COMMAND_SIZE = 7,
    OPERATING_MODE_COMMAND_FAMILY = 0xf8,
    OPERATING_MODE_COMMAND_FORMAT = 9,
    OPERATING_MODE_COMMAND_OPCODE_OFFSET = 2,
    OPERATING_MODE_COMMAND_PARAMETERS_OFFSET = 3,
};

/**
 * @brief Decodes the operating-mode envelope from a short HID output report.
 *
 * Accepts only the F8 09 family and format prefix, then exposes its opcode and four parameter
 * bytes without interpreting the opcode-specific payload.
 *
 * @param[in] output Classified short output report containing the command bytes.
 * @param[out] command Destination for the opcode and four command parameters.
 * @return True when the report kind, size, family, and format fields are accepted.
 */
bool usb_operating_mode_command_decode(const UsbOutputCommand *output,
                                       UsbOperatingModeCommand *command) {
    if (output == NULL || command == NULL || output->kind != USB_OUTPUT_COMMAND_SHORT ||
        output->payload == NULL || output->length != OPERATING_MODE_COMMAND_SIZE ||
        output->payload[0] != OPERATING_MODE_COMMAND_FAMILY ||
        output->payload[1] != OPERATING_MODE_COMMAND_FORMAT) {
        return false;
    }

    command->opcode = output->payload[OPERATING_MODE_COMMAND_OPCODE_OFFSET];
    for (uint8_t index = 0; index < USB_OPERATING_MODE_PARAMETER_COUNT; index++) {
        command->parameters[index] =
            output->payload[OPERATING_MODE_COMMAND_PARAMETERS_OFFSET + index];
    }
    return true;
}

/**
 * @brief Identifies the primary USB reset request.
 *
 * Matches operating-mode opcode 1 with subcommand 1, which selects primary mode 0 before the USB
 * controller restart.
 *
 * @param[in] command Decoded operating-mode command.
 * @return True for the native-mode USB reset request; otherwise false.
 */
bool usb_operating_mode_command_requests_native_reset(const UsbOperatingModeCommand *command) {
    return command != NULL && command->opcode == 1 && command->parameters[0] == 1;
}

/**
 * @brief Decodes an operating-status command.
 *
 * Accepts opcode 2 and converts its first parameter to a boolean operating state. Zero disables
 * the state and every nonzero value enables it.
 *
 * @param[in] command Decoded operating-mode command.
 * @param[out] enabled Destination for the requested operating state.
 * @return True when the command carries an operating-status update.
 */
bool usb_operating_mode_command_decode_status(const UsbOperatingModeCommand *command,
                                              bool *enabled) {
    if (command == NULL || enabled == NULL || command->opcode != 2) {
        return false;
    }
    *enabled = command->parameters[0] != 0;
    return true;
}
