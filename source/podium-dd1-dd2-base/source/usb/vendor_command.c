#include "usb/vendor_command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    VENDOR_COMMAND_SIZE = 63,
    VENDOR_COMMAND_ARGUMENT_SIZE = 62,
    VENDOR_COMMAND_EXTENDED_RESET = 0x0a,
    VENDOR_COMMAND_EXTENDED_RESET_GROUP = 1,
    VENDOR_COMMAND_EXTENDED_RESET_SELECTOR = 0x1a,
};

static int8_t command_kind(uint8_t opcode, const uint8_t *payload) {
    switch (opcode) {
    case 1:
        return USB_VENDOR_COMMAND_DEVICE_CONTROL_RESPONSE;
    case 2:
        return USB_VENDOR_COMMAND_RESPONSE_PREPARATION;
    case 3:
        return USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE;
    case 4:
        return USB_VENDOR_COMMAND_ACKNOWLEDGEMENT;
    case 5:
        return USB_VENDOR_COMMAND_OPERATING_MODE_TRANSITION;
    case 8:
        return USB_VENDOR_COMMAND_STATUS_RESPONSE;
    case VENDOR_COMMAND_EXTENDED_RESET:
        if (payload[1] == VENDOR_COMMAND_EXTENDED_RESET_GROUP &&
            payload[2] == VENDOR_COMMAND_EXTENDED_RESET_SELECTOR) {
            return USB_VENDOR_COMMAND_EXTENDED_RESET;
        }
        return -1;
    case 0x10:
        return USB_VENDOR_COMMAND_EDS_WRITE;
    case 0x11:
    case 0x13:
        return USB_VENDOR_COMMAND_EDS_TRANSFER;
    case 0xff:
        return USB_VENDOR_COMMAND_EXTENDED;
    default:
        return -1;
    }
}

/**
 * Classifies a complete vendor-transfer payload by its top-level command opcode.
 *
 * @param output Classified vendor-transfer output containing 63 command bytes.
 * @param command Destination for the command route, opcode, and remaining arguments.
 * @return True when the opcode selects one of the supported vendor command routes.
 */
bool usb_vendor_command_decode(const UsbOutputCommand *output, UsbVendorCommand *command) {
    if (output == NULL || command == NULL || output->kind != USB_OUTPUT_COMMAND_VENDOR_TRANSFER ||
        output->payload == NULL || output->length != VENDOR_COMMAND_SIZE) {
        return false;
    }

    int8_t kind = command_kind(output->payload[0], output->payload);
    if (kind < 0) {
        return false;
    }

    *command = (UsbVendorCommand){
        .kind = (UsbVendorCommandKind)kind,
        .opcode = output->payload[0],
        .arguments = output->payload + 1,
        .length = VENDOR_COMMAND_ARGUMENT_SIZE,
    };
    return true;
}
