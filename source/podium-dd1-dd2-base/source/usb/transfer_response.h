#ifndef OPENTEC_BASE_USB_TRANSFER_RESPONSE_H
#define OPENTEC_BASE_USB_TRANSFER_RESPONSE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"

enum { USB_TRANSFER_RESPONSE_PAYLOAD_CAPACITY = 124 };

/**
 * @brief Retains one pedal response while it is emitted as USB reports.
 *
 * Tracks payload progress separately from a prepared report so endpoint retries cannot skip or
 * duplicate committed fragments.
 */
typedef struct {
    uint8_t payload[USB_TRANSFER_RESPONSE_PAYLOAD_CAPACITY];
    uint16_t length;
    uint16_t cursor;
    uint8_t sequence;
    uint8_t prepared_length;
    bool prepared;
} UsbTransferResponse;

void usb_transfer_response_init(UsbTransferResponse *response);
bool usb_transfer_response_accepting(const UsbTransferResponse *response);
bool usb_transfer_response_queue(UsbTransferResponse *response, const uint8_t *payload,
                                 uint8_t length);
bool usb_transfer_response_pending(const UsbTransferResponse *response);
bool usb_transfer_response_prepare(UsbTransferResponse *response,
                                   uint8_t output[USB_DEVICE_REPORT_SIZE]);
void usb_transfer_response_commit(UsbTransferResponse *response);

#endif
