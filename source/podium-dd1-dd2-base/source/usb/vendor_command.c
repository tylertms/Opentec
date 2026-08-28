#include "usb/vendor_command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    VENDOR_COMMAND_SIZE = 63,
    VENDOR_COMMAND_ARGUMENT_SIZE = 62,
    VENDOR_COMMAND_EXTENDED_RESET = 0x0a,
    VENDOR_COMMAND_EXTENDED_RESET_GROUP = 1,
    VENDOR_COMMAND_EXTENDED_RESET_SELECTOR = 0x1a,
    VENDOR_COMMAND_WHEEL_TRANSFER = 0xe0,
    VENDOR_COMMAND_WHEEL_TRANSFER_WRITE = 0x0402,
    VENDOR_COMMAND_WHEEL_TRANSFER_READ = 0x0502,
};

static int8_t command_kind(uint8_t opcode, const uint8_t *payload) {
    switch (opcode) {
    case 1:
        return USB_VENDOR_COMMAND_WHEEL_OUTPUT_REPORT;
    case 2:
        return USB_VENDOR_COMMAND_RESPONSE_PREPARATION;
    case 3:
        return USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE;
    case 4:
        return USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT;
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
 * @brief Classify a complete vendor-transfer payload.
 *
 * Selects the command route from the top-level opcode and exposes the remaining 62 bytes as its
 * arguments. Extended reset packets are accepted only with group 1 and selector 0x1A.
 *
 * @param[in] output Classified vendor-transfer output containing 63 command bytes.
 * @param[out] command Destination for the command route, opcode, and remaining arguments.
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

/**
 * @brief Identifies the extended vendor request that starts the motor-command handshake.
 *
 * Matches an extended request whose first three argument bytes are 0, 1, and 1.
 *
 * @param[in] command Decoded vendor command.
 * @return True for extended arguments 00 01 01.
 */
bool usb_vendor_command_requests_motor_command(const UsbVendorCommand *command) {
    return command != NULL && command->kind == USB_VENDOR_COMMAND_EXTENDED &&
           command->arguments != NULL && command->length >= 3 && command->arguments[0] == 0 &&
           command->arguments[1] == 1 && command->arguments[2] == 1;
}

/**
 * @brief Decodes an extended wheel-transfer vendor command.
 *
 * Accepts the E0 route followed by little-endian command 0x0402 or 0x0502 and action one or two.
 *
 * @param[in] command Decoded vendor command.
 * @param[out] transfer Wheel-transfer request and action.
 * @return True when the extended arguments select a supported wheel transfer.
 */
bool usb_vendor_command_decode_wheel_transfer(const UsbVendorCommand *command,
                                              UsbWheelTransferCommand *transfer) {
    if (command == NULL || transfer == NULL || command->kind != USB_VENDOR_COMMAND_EXTENDED ||
        command->arguments == NULL || command->length < 4 ||
        command->arguments[0] != VENDOR_COMMAND_WHEEL_TRANSFER) {
        return false;
    }
    uint16_t value = (uint16_t)command->arguments[1] | (uint16_t)command->arguments[2] << 8;
    if (value != VENDOR_COMMAND_WHEEL_TRANSFER_WRITE &&
        value != VENDOR_COMMAND_WHEEL_TRANSFER_READ) {
        return false;
    }
    uint8_t action = command->arguments[3];
    if (action != USB_WHEEL_TRANSFER_START && action != USB_WHEEL_TRANSFER_STATUS) {
        return false;
    }
    transfer->request =
        value == VENDOR_COMMAND_WHEEL_TRANSFER_READ ? WHEEL_TRANSFER_READ : WHEEL_TRANSFER_WRITE;
    transfer->action = (UsbWheelTransferAction)action;
    return true;
}

/**
 * @brief Encodes an extended wheel-transfer status response.
 *
 * Clears a 64-byte vendor report and writes the FF E0 route, the selected little-endian command,
 * and its signed status byte into the first five positions.
 *
 * @param[in] request Write or read request channel.
 * @param[in] status Current wheel-transfer status.
 * @param[out] output Encoded 64-byte vendor report.
 */
void usb_vendor_command_encode_wheel_transfer_response(WheelTransferRequest request,
                                                       WheelTransferStatus status,
                                                       uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        output[index] = 0;
    }
    uint16_t value = request == WHEEL_TRANSFER_READ ? VENDOR_COMMAND_WHEEL_TRANSFER_READ
                                                    : VENDOR_COMMAND_WHEEL_TRANSFER_WRITE;
    output[0] = UINT8_MAX;
    output[1] = VENDOR_COMMAND_WHEEL_TRANSFER;
    output[2] = (uint8_t)value;
    output[3] = (uint8_t)(value >> 8);
    output[4] = (uint8_t)status;
}
