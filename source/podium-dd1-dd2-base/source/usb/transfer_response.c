#include "usb/transfer_response.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Transfer response wire-format constants. */
enum {
    TRANSFER_RESPONSE_PREFIX = 0xff,        /**< Prefix byte for every transfer response report. */
    TRANSFER_RESPONSE_SINGLE = 0x10,        /**< Single-report response form. */
    TRANSFER_RESPONSE_FIRST = 0x11,         /**< First segmented response form. */
    TRANSFER_RESPONSE_CONTINUE = 0x12,      /**< Intermediate segmented response form. */
    TRANSFER_RESPONSE_FINAL = 0x13,         /**< Final segmented response form. */
    TRANSFER_RESPONSE_SINGLE_CAPACITY = 62, /**< Payload bytes available in a single report. */
    TRANSFER_RESPONSE_FIRST_CAPACITY =
        60, /**< Payload bytes available after a first-report header. */
    TRANSFER_RESPONSE_CONTINUE_CAPACITY =
        61, /**< Payload bytes available after a continuation header. */
};

/**
 * @brief Selects the smaller of a remaining payload and report capacity.
 *
 * Bounds the next fragment without changing response progress.
 *
 * @param[in] remaining Payload bytes not yet committed.
 * @param[in] capacity Bytes available in the selected report format.
 * @return Number of payload bytes carried by the report.
 */
static uint8_t fragment_length(uint16_t remaining, uint8_t capacity) {
    return remaining < capacity ? (uint8_t)remaining : capacity;
}

void usb_transfer_response_init(UsbTransferResponse *response) {
    *response = (UsbTransferResponse){0};
}

bool usb_transfer_response_accepting(const UsbTransferResponse *response) {
    return response != NULL && response->cursor == response->length && !response->prepared;
}

bool usb_transfer_response_queue(UsbTransferResponse *response, const uint8_t *payload,
                                 uint8_t length) {
    if (!usb_transfer_response_accepting(response) || payload == NULL || length == 0 ||
        length > sizeof(response->payload)) {
        return false;
    }
    memcpy(response->payload, payload, length);
    response->length = length;
    response->cursor = 0;
    response->sequence = 0;
    return true;
}

bool usb_transfer_response_pending(const UsbTransferResponse *response) {
    return response != NULL && response->cursor < response->length;
}

bool usb_transfer_response_prepare(UsbTransferResponse *response,
                                   uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    if (response == NULL || output == NULL || !usb_transfer_response_pending(response)) {
        return false;
    }

    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    uint16_t remaining = response->length - response->cursor;
    output[0] = TRANSFER_RESPONSE_PREFIX;
    if (response->cursor == 0 && response->length <= TRANSFER_RESPONSE_SINGLE_CAPACITY) {
        output[1] = TRANSFER_RESPONSE_SINGLE;
        response->prepared_length = (uint8_t)remaining;
        memcpy(output + 2, response->payload, response->prepared_length);
    } else if (response->cursor == 0) {
        output[1] = TRANSFER_RESPONSE_FIRST;
        output[2] = (uint8_t)(response->length >> 8);
        output[3] = (uint8_t)response->length;
        response->prepared_length = fragment_length(remaining, TRANSFER_RESPONSE_FIRST_CAPACITY);
        memcpy(output + 4, response->payload, response->prepared_length);
    } else {
        output[1] = remaining <= TRANSFER_RESPONSE_CONTINUE_CAPACITY ? TRANSFER_RESPONSE_FINAL
                                                                     : TRANSFER_RESPONSE_CONTINUE;
        output[2] = response->sequence;
        response->prepared_length = fragment_length(remaining, TRANSFER_RESPONSE_CONTINUE_CAPACITY);
        memcpy(output + 3, response->payload + response->cursor, response->prepared_length);
    }
    response->prepared = true;
    return true;
}

void usb_transfer_response_commit(UsbTransferResponse *response) {
    if (response == NULL || !response->prepared) {
        return;
    }
    bool first_fragment =
        response->cursor == 0 && response->length > TRANSFER_RESPONSE_SINGLE_CAPACITY;
    response->cursor += response->prepared_length;
    response->sequence = first_fragment ? 1 : (uint8_t)(response->sequence + 1);
    response->prepared_length = 0;
    response->prepared = false;
    if (response->cursor >= response->length) {
        response->length = 0;
        response->cursor = 0;
        response->sequence = 0;
    }
}
