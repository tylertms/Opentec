#ifndef OPENTEC_BASE_USB_TRANSFER_REQUEST_H
#define OPENTEC_BASE_USB_TRANSFER_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/vendor_command.h"

enum { USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY = 124 };

/**
 * @brief Stores one complete logical request received through USB transfer reports.
 *
 * Removes the tuning-menu framing before the request is forwarded to the pedal controller.
 */
typedef struct {
    uint8_t data[USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY];
    uint8_t length;
} UsbTransferRequestPayload;

/**
 * @brief Reassembles tuning-menu transfer request reports.
 *
 * Supports the single, first, and final command forms while retaining an incomplete request across
 * reports.
 */
typedef struct {
    UsbTransferRequestPayload payload;
    uint8_t cursor;
    bool active;
    bool ready;
} UsbTransferRequest;

void usb_transfer_request_init(UsbTransferRequest *request);
bool usb_transfer_request_apply(UsbTransferRequest *request, const UsbVendorCommand *command);
const UsbTransferRequestPayload *usb_transfer_request_payload(const UsbTransferRequest *request);
void usb_transfer_request_release(UsbTransferRequest *request);

#endif
