#include "usb/transfer_request.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    TRANSFER_REQUEST_SINGLE = 0x10,
    TRANSFER_REQUEST_FIRST = 0x11,
    TRANSFER_REQUEST_FINAL = 0x13,
    TRANSFER_REQUEST_SINGLE_HEADER_SIZE = 3,
    TRANSFER_REQUEST_FIRST_CAPACITY = 60,
    TRANSFER_REQUEST_FINAL_CAPACITY = 62,
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

/**
 * @brief Initializes an empty USB transfer request reassembler.
 *
 * Clears retained payload, cursor, and completion state.
 *
 * @param[out] request Request reassembler to initialize.
 */
void usb_transfer_request_init(UsbTransferRequest *request) { *request = (UsbTransferRequest){0}; }

/**
 * @brief Applies one tuning-menu transfer request report.
 *
 * Single reports carry their declared logical payload directly. First reports start a big-endian
 * length-bearing request, and one or more final reports append their sequence-bearing payload until
 * the declared length is complete. Unsupported tuning commands are not claimed.
 *
 * @param[in,out] request Request reassembly state.
 * @param[in] command Decoded vendor command containing the tuning-menu transfer report.
 * @return True when the command selects a single, first, or final transfer form.
 */
bool usb_transfer_request_apply(UsbTransferRequest *request, const UsbVendorCommand *command) {
    if (request == NULL || command == NULL || command->kind != USB_VENDOR_COMMAND_TUNING_MENU ||
        command->arguments == NULL || command->length == 0) {
        return false;
    }

    uint8_t type = command->arguments[0];
    if (type != TRANSFER_REQUEST_SINGLE && type != TRANSFER_REQUEST_FIRST &&
        type != TRANSFER_REQUEST_FINAL) {
        return false;
    }
    if (request->ready) {
        return true;
    }

    if (type == TRANSFER_REQUEST_SINGLE) {
        if (command->length < 3) {
            return true;
        }
        uint16_t declared = (uint16_t)command->arguments[2] + TRANSFER_REQUEST_SINGLE_HEADER_SIZE;
        uint8_t available = (uint8_t)(command->length - 2u);
        if (declared > USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY || declared > available) {
            return true;
        }
        uint8_t length = (uint8_t)declared;
        memcpy(request->payload.data, command->arguments + 2, length);
        request->payload.length = length;
        request->ready = true;
        return true;
    }

    if (type == TRANSFER_REQUEST_FIRST) {
        if (command->length < 3) {
            return true;
        }
        uint16_t declared = (uint16_t)command->arguments[1] << 8 | command->arguments[2];
        if (declared > USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY) {
            return true;
        }
        request->payload.length = (uint8_t)declared;
        request->cursor = 0;
        request->active = true;
        uint8_t available = (uint8_t)(command->length - 3u);
        uint8_t length =
            fragment_length(request->payload.length, available, TRANSFER_REQUEST_FIRST_CAPACITY);
        memcpy(request->payload.data, command->arguments + 3, length);
        request->cursor = length;
        return true;
    }

    if (!request->active || command->length < 2) {
        return true;
    }
    if (request->cursor >= request->payload.length) {
        request->active = false;
        return true;
    }
    uint8_t remaining = (uint8_t)(request->payload.length - request->cursor);
    uint8_t available = (uint8_t)(command->length - 2u);
    uint8_t length = fragment_length(remaining, available, TRANSFER_REQUEST_FINAL_CAPACITY);
    memcpy(request->payload.data + request->cursor, command->arguments + 2, length);
    request->cursor = (uint8_t)(request->cursor + length);
    if (request->cursor == request->payload.length) {
        request->active = false;
        request->ready = true;
    }
    return true;
}

/**
 * @brief Provides the completed logical USB transfer request.
 *
 * Keeps the completed payload stable until the caller releases it.
 *
 * @param[in] request Request reassembly state.
 * @return Completed request, or null while no request is ready.
 */
const UsbTransferRequestPayload *usb_transfer_request_payload(const UsbTransferRequest *request) {
    return request != NULL && request->ready ? &request->payload : NULL;
}

/**
 * @brief Releases the completed USB transfer request.
 *
 * Leaves any independently active segmented request intact and opens the completed payload slot.
 *
 * @param[in,out] request Request reassembly state to release.
 */
void usb_transfer_request_release(UsbTransferRequest *request) {
    if (request != NULL) {
        request->payload.length = 0;
        request->ready = false;
    }
}
