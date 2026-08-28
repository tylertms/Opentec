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
