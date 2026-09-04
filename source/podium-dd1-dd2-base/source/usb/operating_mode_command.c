#include "usb/operating_mode_command.h"

#include <stddef.h>
#include <stdint.h>

/** @brief Internal offsets, opcodes, and values for operating-mode commands. */
enum {
    OPERATING_MODE_COMMAND_SIZE = 7,              /**< Short command payload size in bytes. */
    OPERATING_MODE_COMMAND_FAMILY = 0xf8,         /**< Operating-mode command family byte. */
    OPERATING_MODE_COMMAND_FORMAT = 9,            /**< Operating-mode command format byte. */
    OPERATING_MODE_COMMAND_OPCODE_OFFSET = 2,     /**< Opcode offset in the short payload. */
    OPERATING_MODE_COMMAND_PARAMETERS_OFFSET = 3, /**< Parameter offset in the short payload. */
    OPERATING_MODE_LED_PATTERN_IF_CLEAR_OPCODE =
        0x10,                                 /**< Conditional LED-pattern command opcode. */
    OPERATING_MODE_LED_PATTERN_OPCODE = 0x11, /**< Unconditional LED-pattern command opcode. */
    OPERATING_MODE_RUNTIME_OPCODE = 1,        /**< Runtime-transition command opcode. */
    OPERATING_MODE_RUNTIME_SUBCOMMAND = 0xfe, /**< Runtime-transition subcommand. */
    OPERATING_MODE_RESET_SUBCOMMAND = 0xff,   /**< Reset-transition subcommand. */
    OPERATING_MODE_RUNTIME_INTERFACE_LIMIT =
        1, /**< Maximum interface mode for runtime transitions. */
    OPERATING_MODE_AUXILIARY_STANDARD_STATE = 2, /**< Auxiliary standard state value. */
    OPERATING_MODE_AUXILIARY_RECOVERY_STATE = 3, /**< Auxiliary recovery state value. */
};

/**
 * @brief Reports whether a wheel mode permits the USB bridge.
 *
 * Accepts boot mode, legacy modes 9 through 12, standard modes 16 through 22, and extended modes
 * 27 through 30.
 *
 * @param[in] wheel_mode Current attached-wheel mode.
 * @return True when the wheel mode permits a USB bridge transition; otherwise false.
 */
static bool wheel_mode_supports_usb_bridge(uint8_t wheel_mode) {
    return wheel_mode == 0 || (wheel_mode >= 9 && wheel_mode <= 12) ||
           (wheel_mode >= 16 && wheel_mode <= 22) || (wheel_mode >= 27 && wheel_mode <= 30);
}

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

bool usb_operating_mode_command_requests_native_reset(const UsbOperatingModeCommand *command) {
    return command != NULL && command->opcode == 1 && command->parameters[0] == 1;
}

bool usb_operating_mode_command_is_noop(const UsbOperatingModeCommand *command) {
    return command != NULL && command->opcode == 2;
}

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
