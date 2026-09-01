#include "usb/vendor_command.h"

#include <stddef.h>
#include <stdint.h>

/** @brief Private framing values and selectors used by vendor command decoding. */
enum {
    VENDOR_COMMAND_SIZE = 63,                   /**< Maximum vendor command payload size. */
    VENDOR_COMMAND_ARGUMENT_SIZE = 62,          /**< Maximum argument size after the opcode. */
    VENDOR_COMMAND_SCRIPT = 0x0a,               /**< Script command opcode. */
    VENDOR_COMMAND_SCRIPT_GROUP = 0,            /**< Supported script command group. */
    VENDOR_COMMAND_SCRIPT_SAMPLES_SELECTOR = 4, /**< Script samples selector. */
    VENDOR_COMMAND_SCRIPT_SAMPLES_LAST_FIRST = 0x01f5, /**< Largest accepted first sample index. */
    VENDOR_COMMAND_SCRIPT_SLOT_SELECTOR = 5,           /**< Script slot selector. */
    VENDOR_COMMAND_SCRIPT_SLOT_LAST = 14,         /**< Largest accepted reportable script slot. */
    VENDOR_COMMAND_SCRIPT_STATUS_SELECTOR = 6,    /**< Script status selector. */
    VENDOR_COMMAND_SCRIPT_VALUES_SELECTOR = 7,    /**< Script values selector. */
    VENDOR_COMMAND_SCRIPT_AXES_SELECTOR = 8,      /**< Script axes selector. */
    VENDOR_COMMAND_WHEEL_TRANSFER = 0xe0,         /**< Extended wheel-transfer route. */
    VENDOR_COMMAND_WHEEL_TRANSFER_WRITE = 0x0402, /**< Extended write-transfer command value. */
    VENDOR_COMMAND_WHEEL_TRANSFER_READ = 0x0502,  /**< Extended read-transfer command value. */
    VENDOR_COMMAND_TUNING_MENU_REPORT = 1,        /**< Tuning-menu report-17 action. */
};

/**
 * @brief Selects the route for a vendor command opcode.
 *
 * Maps each supported top-level opcode to its command category and rejects opcode 0x0A
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
        return USB_VENDOR_COMMAND_TUNING_STATUS;
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
    case 0x10:
    case 0x11:
    case 0x13:
        return USB_VENDOR_COMMAND_TRANSFER_REQUEST;
    case 0xff:
        return USB_VENDOR_COMMAND_EXTENDED;
    default:
        return -1;
    }
}

bool usb_vendor_command_script_sample_index(const UsbVendorCommand *command, uint16_t *index) {
    if (command == NULL || index == NULL || command->kind != USB_VENDOR_COMMAND_SCRIPT_SAMPLES ||
        command->arguments == NULL || command->length < 6) {
        return false;
    }
    *index = (uint16_t)command->arguments[4] | (uint16_t)((uint16_t)command->arguments[5] << 8);
    return *index <= VENDOR_COMMAND_SCRIPT_SAMPLES_LAST_FIRST;
}

bool usb_vendor_command_script_slot_index(const UsbVendorCommand *command, uint8_t *index) {
    if (command == NULL || index == NULL || command->kind != USB_VENDOR_COMMAND_SCRIPT_SLOT ||
        command->arguments == NULL || command->length < 5) {
        return false;
    }
    *index = command->arguments[4];
    return *index <= VENDOR_COMMAND_SCRIPT_SLOT_LAST;
}

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

bool usb_vendor_command_requests_motor_command(const UsbVendorCommand *command) {
    return command != NULL && command->kind == USB_VENDOR_COMMAND_EXTENDED &&
           command->arguments != NULL && command->length >= 3 && command->arguments[0] == 0 &&
           command->arguments[1] == 1 && command->arguments[2] == 1;
}

/**
 * @brief Tests for the official extended auxiliary-menu command signature.
 *
 * @param[in] command Decoded vendor command.
 * @return True for the auxiliary-menu signature; otherwise false.
 */
bool usb_vendor_command_requests_auxiliary_menu(const UsbVendorCommand *command) {
    return command != NULL && command->kind == USB_VENDOR_COMMAND_EXTENDED &&
           command->arguments != NULL && command->length >= 3 && command->arguments[0] == 0 &&
           command->arguments[1] == 0 && command->arguments[2] == 1;
}

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

const uint8_t *usb_vendor_command_decode_wheel_report_seventeen(const UsbVendorCommand *command) {
    if (command == NULL || command->kind != USB_VENDOR_COMMAND_TUNING_MENU ||
        command->arguments == NULL || command->length < VENDOR_COMMAND_ARGUMENT_SIZE ||
        command->arguments[0] != VENDOR_COMMAND_TUNING_MENU_REPORT) {
        return NULL;
    }
    return command->arguments + 1;
}

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
