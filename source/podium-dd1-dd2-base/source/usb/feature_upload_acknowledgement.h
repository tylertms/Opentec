#ifndef OPENTEC_BASE_USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_H
#define OPENTEC_BASE_USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_upload.h"

enum {
    USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE = 13,
};

bool usb_feature_upload_acknowledgement_segmented_encode(
    uint8_t sequence, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE], uint16_t transferred,
    uint16_t total_length, uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE]);
bool usb_feature_upload_acknowledgement_compact_encode(
    uint8_t sequence, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE]);

#endif
