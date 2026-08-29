#include "usb/operating_mode_command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    OPERATING_MODE_COMMAND_SIZE = 7,
    OPERATING_MODE_COMMAND_FAMILY = 0xf8,
    OPERATING_MODE_COMMAND_FORMAT = 9,
    OPERATING_MODE_COMMAND_OPCODE_OFFSET = 2,
    OPERATING_MODE_COMMAND_PARAMETERS_OFFSET = 3,
    OPERATING_MODE_LED_PATTERN_IF_CLEAR_OPCODE = 0x10,
    OPERATING_MODE_LED_PATTERN_OPCODE = 0x11,
    OPERATING_MODE_RUNTIME_OPCODE = 1,
    OPERATING_MODE_RUNTIME_SUBCOMMAND = 0xfe,
    OPERATING_MODE_RESET_SUBCOMMAND = 0xff,
    OPERATING_MODE_RUNTIME_INTERFACE_LIMIT = 1,
    OPERATING_MODE_AUXILIARY_STANDARD_STATE = 2,
    OPERATING_MODE_AUXILIARY_RECOVERY_STATE = 3,
};

/**
 * @brief Reports whether a wheel mode permits the USB bridge.
 *
 * Accepts boot mode, legacy modes 9 through 12, standard modes 16 through 22, and extended modes
 * 27 through 30.
 *
 * @param[in] wheel_mode Current attached-wheel mode.
 * @return True when the wheel mode permits a USB bridge transition.
 */
static bool wheel_mode_supports_usb_bridge(uint8_t wheel_mode) {
    return wheel_mode == 0 || (wheel_mode >= 9 && wheel_mode <= 12) ||
           (wheel_mode >= 16 && wheel_mode <= 22) || (wheel_mode >= 27 && wheel_mode <= 30);
}

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

/**
 * @brief Identifies an accepted board LED pattern command.
 *
 * Accepts opcode 0x10 only when its first parameter is zero and accepts opcode 0x11 without that
 * gate. Both commands carry the eight-bit LED pattern in their second parameter.
 *
 * @param[in] command Decoded operating-mode command.
 * @return True when the command carries an accepted LED pattern update.
 */
bool usb_operating_mode_command_requests_led_pattern(const UsbOperatingModeCommand *command) {
    if (command == NULL) {
        return false;
    }
    if (command->opcode == OPERATING_MODE_LED_PATTERN_IF_CLEAR_OPCODE &&
        command->parameters[0] != 0) {
        return false;
    }
    return command->opcode == OPERATING_MODE_LED_PATTERN_IF_CLEAR_OPCODE ||
           command->opcode == OPERATING_MODE_LED_PATTERN_OPCODE;
}

/**
 * @brief Decodes a runtime-mode transition request.
 *
 * Accepts reset independently of the active interface. Other transitions require interface mode
 * zero or one. Auxiliary requests select their normal or recovery mode from auxiliary state two
 * or three and request a settings save. USB bridge requests additionally require a supported
 * attached-wheel mode.
 *
 * @param[in] command Decoded operating-mode command.
 * @param[in] interface_mode Active host interface mode.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] auxiliary_state Current auxiliary interface state.
 * @param[out] transition Accepted runtime mode and pre-transition settings action.
 * @return True when the command requests an allowed runtime transition.
 */
bool usb_operating_mode_command_decode_runtime(const UsbOperatingModeCommand *command,
                                               uint8_t interface_mode, uint8_t wheel_mode,
                                               uint8_t auxiliary_state,
                                               UsbRuntimeModeTransition *transition) {
    if (command == NULL || transition == NULL || command->opcode != OPERATING_MODE_RUNTIME_OPCODE) {
        return false;
    }
    if (command->parameters[0] == OPERATING_MODE_RESET_SUBCOMMAND) {
        *transition = (UsbRuntimeModeTransition){.mode = USB_RUNTIME_MODE_RESET};
        return true;
    }
    if (command->parameters[0] != OPERATING_MODE_RUNTIME_SUBCOMMAND ||
        interface_mode > OPERATING_MODE_RUNTIME_INTERFACE_LIMIT) {
        return false;
    }

    switch (command->parameters[1]) {
    case 0:
        if (auxiliary_state == OPERATING_MODE_AUXILIARY_STANDARD_STATE) {
            *transition = (UsbRuntimeModeTransition){
                .mode = USB_RUNTIME_MODE_AUXILIARY,
                .save_settings = true,
            };
            return true;
        }
        if (auxiliary_state == OPERATING_MODE_AUXILIARY_RECOVERY_STATE) {
            *transition = (UsbRuntimeModeTransition){
                .mode = USB_RUNTIME_MODE_AUXILIARY_RECOVERY,
                .save_settings = true,
            };
            return true;
        }
        return false;
    case 1:
        *transition = (UsbRuntimeModeTransition){.mode = USB_RUNTIME_MODE_STATUS_BRIDGE};
        return true;
    case 2:
        if (!wheel_mode_supports_usb_bridge(wheel_mode)) {
            return false;
        }
        *transition = (UsbRuntimeModeTransition){.mode = USB_RUNTIME_MODE_USB_BRIDGE};
        return true;
    case 3:
        *transition = (UsbRuntimeModeTransition){.mode = USB_RUNTIME_MODE_PROTOCOL_BRIDGE};
        return true;
    default:
        return false;
    }
}
