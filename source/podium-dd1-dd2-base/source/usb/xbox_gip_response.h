#ifndef OPENTEC_BASE_USB_XBOX_GIP_RESPONSE_H
#define OPENTEC_BASE_USB_XBOX_GIP_RESPONSE_H

#include <stdint.h>

#include "usb/console_descriptor.h"

enum {
    USB_XBOX_GIP_DIGEST_RESPONSE_SIZE = 32,
    USB_XBOX_GIP_READY_RESPONSE_SIZE = 8,
    USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE = 13,
};

uint8_t usb_xbox_gip_sequence_take(uint8_t *next_sequence);
void usb_xbox_gip_digest_response_encode(BoardVariant variant, uint8_t wheel_mode, uint8_t sequence,
                                         const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                         uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE]);
void usb_xbox_gip_ready_response_encode(uint8_t sequence,
                                        uint8_t output[USB_XBOX_GIP_READY_RESPONSE_SIZE]);
void usb_xbox_gip_transfer_status_response_encode(
    uint8_t sequence, const uint8_t request[2],
    uint8_t output[USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE]);

#endif
