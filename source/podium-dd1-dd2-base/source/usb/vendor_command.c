#include "usb/vendor_command.h"

#include <stddef.h>
#include <stdint.h>

enum {
    VENDOR_COMMAND_SIZE = 63,
    VENDOR_COMMAND_ARGUMENT_SIZE = 62,
    VENDOR_COMMAND_SCRIPT = 0x0a,
    VENDOR_COMMAND_SCRIPT_GROUP = 0,
    VENDOR_COMMAND_SCRIPT_SAMPLES_SELECTOR = 4,
    VENDOR_COMMAND_SCRIPT_SAMPLES_LAST_FIRST = 0x01f5,
    VENDOR_COMMAND_SCRIPT_SLOT_SELECTOR = 5,
    VENDOR_COMMAND_SCRIPT_SLOT_LAST = 14,
    VENDOR_COMMAND_SCRIPT_STATUS_SELECTOR = 6,
    VENDOR_COMMAND_SCRIPT_VALUES_SELECTOR = 7,
    VENDOR_COMMAND_SCRIPT_AXES_SELECTOR = 8,
    VENDOR_COMMAND_WHEEL_TRANSFER = 0xe0,
    VENDOR_COMMAND_WHEEL_TRANSFER_WRITE = 0x0402,
    VENDOR_COMMAND_WHEEL_TRANSFER_READ = 0x0502,
    VENDOR_COMMAND_TUNING_MENU_REPORT = 1,
};

/**
 * @brief Selects the route for a vendor command opcode.
 *
 * Maps each supported top-level opcode to its clean command category and rejects opcode 0x0A
 * unless its group and selector match a supported script query.
 *
 * @param[in] opcode Top-level vendor command opcode.
 * @param[in] payload Vendor command payload beginning with its opcode.
 * @param[in] length Number of available payload bytes.
 * @return Command category value, or negative one when the command is unsupported.
 */
static int8_t command_kind(uint8_t opcode, const uint8_t *payload, uint8_t length) {
    switch (opcode) {
    case 1:
        return USB_VENDOR_COMMAND_WHEEL_OUTPUT_REPORT;
    case 2:
        return USB_VENDOR_COMMAND_TUNING_MENU;
    case 3:
        return USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE;
    case 4:
        return USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT;
    case 5:
        return USB_VENDOR_COMMAND_REMOTE_TUNING;
    case 8:
        return USB_VENDOR_COMMAND_STATUS_RESPONSE;
    case VENDOR_COMMAND_SCRIPT:
        if (length >= 7 && payload[1] == VENDOR_COMMAND_SCRIPT_GROUP) {
            if (payload[4] == VENDOR_COMMAND_SCRIPT_SAMPLES_SELECTOR &&
                ((uint16_t)payload[5] | (uint16_t)((uint16_t)payload[6] << 8)) <=
                    VENDOR_COMMAND_SCRIPT_SAMPLES_LAST_FIRST) {
                return USB_VENDOR_COMMAND_SCRIPT_SAMPLES;
            }
            if (payload[4] == VENDOR_COMMAND_SCRIPT_SLOT_SELECTOR &&
                payload[5] <= VENDOR_COMMAND_SCRIPT_SLOT_LAST) {
                return USB_VENDOR_COMMAND_SCRIPT_SLOT;
            }
            if (payload[4] == VENDOR_COMMAND_SCRIPT_STATUS_SELECTOR) {
                return USB_VENDOR_COMMAND_SCRIPT_STATUS;
            }
            if (payload[4] == VENDOR_COMMAND_SCRIPT_VALUES_SELECTOR) {
                return USB_VENDOR_COMMAND_SCRIPT_VALUES;
            }
            if (payload[4] == VENDOR_COMMAND_SCRIPT_AXES_SELECTOR) {
                return USB_VENDOR_COMMAND_SCRIPT_AXES;
            }
        }
        return -1;
    case 0xff:
        return USB_VENDOR_COMMAND_EXTENDED;
    default:
        return -1;
    }
}

/**
 * @brief Reads the first sample index from a script sample query.
 *
 * Combines command argument bytes 4 and 5 in least-significant-byte-first order after confirming
 * that the decoded command selects the bounded ten-sample response.
 *
 * @param[in] command Decoded script sample query.
 * @param[out] index Destination for the first sample index.
 * @return True when the command contains a valid sample query index.
 */
bool usb_vendor_command_script_sample_index(const UsbVendorCommand *command, uint16_t *index) {
    if (command == NULL || index == NULL || command->kind != USB_VENDOR_COMMAND_SCRIPT_SAMPLES ||
        command->arguments == NULL || command->length < 6) {
        return false;
    }
    *index = (uint16_t)command->arguments[4] | (uint16_t)((uint16_t)command->arguments[5] << 8);
    return *index <= VENDOR_COMMAND_SCRIPT_SAMPLES_LAST_FIRST;
}

/**
 * @brief Reads the slot index from a script slot query.
 *
 * Reads command argument byte 4 after confirming that the decoded command selects a reportable
 * script slot from zero through 14.
 *
 * @param[in] command Decoded script slot query.
 * @param[out] index Destination for the slot index.
 * @return True when the command contains a reportable script slot index.
 */
bool usb_vendor_command_script_slot_index(const UsbVendorCommand *command, uint8_t *index) {
    if (command == NULL || index == NULL || command->kind != USB_VENDOR_COMMAND_SCRIPT_SLOT ||
        command->arguments == NULL || command->length < 5) {
        return false;
    }
    *index = command->arguments[4];
    return *index <= VENDOR_COMMAND_SCRIPT_SLOT_LAST;
}

/**
 * @brief Classifies a vendor-transfer payload.
 *
 * Selects the command route from the top-level opcode and exposes the remaining bytes as its
 * arguments. Native payloads provide 63 bytes and the Xbox vendor tunnel provides 59. Opcode 0x0A
 * packets are accepted only when all seven query bytes are present with a supported signature.
 *
 * @param[in] output Classified vendor-transfer output containing up to 63 command bytes.
 * @param[out] command Destination for the command route, opcode, and remaining arguments.
 * @return True when the opcode selects one of the supported vendor command routes.
 */
bool usb_vendor_command_decode(const UsbOutputCommand *output, UsbVendorCommand *command) {
    if (output == NULL || command == NULL || output->kind != USB_OUTPUT_COMMAND_VENDOR_TRANSFER ||
        output->payload == NULL || output->length == 0 || output->length > VENDOR_COMMAND_SIZE) {
        return false;
    }

    int8_t kind = command_kind(output->payload[0], output->payload, output->length);
    if (kind < 0) {
        return false;
    }

    *command = (UsbVendorCommand){
        .kind = (UsbVendorCommandKind)kind,
        .opcode = output->payload[0],
        .arguments = output->payload + 1,
        .length = (uint8_t)(output->length - 1u),
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
 * @brief Decodes a tuning-menu attached-wheel report transfer.
 *
 * Accepts tuning-menu action one and exposes its complete 61-byte report payload.
 *
 * @param[in] command Decoded vendor command.
 * @return Complete report 17 payload, or null when the command does not contain tuning-menu action
 * one and all 61 payload bytes.
 */
const uint8_t *usb_vendor_command_decode_wheel_report_seventeen(const UsbVendorCommand *command) {
    if (command == NULL || command->kind != USB_VENDOR_COMMAND_TUNING_MENU ||
        command->arguments == NULL || command->length < VENDOR_COMMAND_ARGUMENT_SIZE ||
        command->arguments[0] != VENDOR_COMMAND_TUNING_MENU_REPORT) {
        return NULL;
    }
    return command->arguments + 1;
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
