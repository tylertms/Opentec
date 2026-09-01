#include "usb/transfer_request.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Transfer request wire-format constants. */
enum {
    TRANSFER_REQUEST_SINGLE = 0x10, /**< Single transfer request form. */
    TRANSFER_REQUEST_FIRST = 0x11,  /**< First segmented transfer request form. */
    TRANSFER_REQUEST_FINAL = 0x13,  /**< Final or continuation segmented request form. */
    TRANSFER_REQUEST_SINGLE_HEADER_SIZE =
        3, /**< Header bytes included in a single request length. */
    TRANSFER_REQUEST_FIRST_CAPACITY =
        60, /**< Payload bytes available after a first-fragment header. */
    TRANSFER_REQUEST_FINAL_CAPACITY =
        61, /**< Payload bytes available after a continuation/final header. */
};

/**
 * @brief Selects the smallest available request fragment length.
 *
 * Bounds each copy by the logical remainder, bytes present, and fragment format capacity.
 *
 * @param[in] remaining Bytes still required by the logical request.
 * @param[in] available Bytes present in the USB command.
 * @param[in] capacity Maximum bytes consumed by the fragment form.
 * @return Bytes to copy from this fragment.
 */
static uint8_t fragment_length(uint8_t remaining, uint8_t available, uint8_t capacity) {
    uint8_t length = remaining < available ? remaining : available;
    return length < capacity ? length : capacity;
}

void usb_transfer_request_init(UsbTransferRequest *request) { *request = (UsbTransferRequest){0}; }

bool usb_transfer_request_apply(UsbTransferRequest *request, const UsbVendorCommand *command) {
    if (request == NULL || command == NULL || command->arguments == NULL || command->length == 0 ||
        (command->kind != USB_VENDOR_COMMAND_TUNING_MENU &&
         command->kind != USB_VENDOR_COMMAND_TRANSFER_REQUEST)) {
        return false;
    }

    bool direct = command->kind == USB_VENDOR_COMMAND_TRANSFER_REQUEST;
    uint8_t type = direct ? command->opcode : command->arguments[0];
    if (type != TRANSFER_REQUEST_SINGLE && type != TRANSFER_REQUEST_FIRST &&
        type != TRANSFER_REQUEST_FINAL) {
        return false;
    }
    if (request->ready) {
        return true;
    }

    if (type == TRANSFER_REQUEST_SINGLE) {
        uint8_t length_index = direct ? 0 : 2;
        if (command->length <= length_index) {
            return true;
        }
        uint16_t declared =
            (uint16_t)command->arguments[length_index] + TRANSFER_REQUEST_SINGLE_HEADER_SIZE;
        uint8_t available = (uint8_t)(command->length - length_index);
        if (declared > USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY || declared > available) {
            return true;
        }
        uint8_t length = (uint8_t)declared;
        memcpy(request->payload.data, command->arguments + length_index, length);
        request->payload.length = length;
        request->ready = true;
        return true;
    }

    if (type == TRANSFER_REQUEST_FIRST) {
        uint8_t length_index = direct ? 0 : 1;
        if (command->length < (uint8_t)(length_index + 2u)) {
            return true;
        }
        uint16_t declared =
            (uint16_t)command->arguments[length_index] << 8 | command->arguments[length_index + 1u];
        if (declared > USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY) {
            return true;
        }
        request->payload.length = (uint8_t)declared;
        request->cursor = 0;
        request->active = true;
        uint8_t data_index = (uint8_t)(length_index + 2u);
        uint8_t available = (uint8_t)(command->length - data_index);
        uint8_t length =
            fragment_length(request->payload.length, available, TRANSFER_REQUEST_FIRST_CAPACITY);
        memcpy(request->payload.data, command->arguments + data_index, length);
        request->cursor = length;
        return true;
    }

    uint8_t data_index = direct ? 1 : 2;
    if (!request->active || command->length <= data_index) {
        return true;
    }
    if (request->cursor >= request->payload.length) {
        request->active = false;
        return true;
    }
    uint8_t remaining = (uint8_t)(request->payload.length - request->cursor);
    uint8_t available = (uint8_t)(command->length - data_index);
    uint8_t length = fragment_length(remaining, available, TRANSFER_REQUEST_FINAL_CAPACITY);
    memcpy(request->payload.data + request->cursor, command->arguments + data_index, length);
    request->cursor = (uint8_t)(request->cursor + length);
    if (request->cursor == request->payload.length) {
        request->active = false;
        request->ready = true;
    }
    return true;
}

const UsbTransferRequestPayload *usb_transfer_request_payload(const UsbTransferRequest *request) {
    return request != NULL && request->ready ? &request->payload : NULL;
}

void usb_transfer_request_release(UsbTransferRequest *request) {
    if (request != NULL) {
        request->payload.length = 0;
        request->ready = false;
    }
}
