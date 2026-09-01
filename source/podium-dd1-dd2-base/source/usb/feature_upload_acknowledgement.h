#ifndef OPENTEC_BASE_USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_H
#define OPENTEC_BASE_USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_upload.h"

/** @brief Feature-upload acknowledgement packet size in bytes. */
enum {
    USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE = 13, /**< Acknowledgement packet size. */
};

/**
 * @brief Encodes a segmented feature-upload acknowledgement.
 *
 * Echoes the report and transfer type and carries assembled and remaining byte counts.
 *
 * @param[in] sequence Vendor HID response sequence.
 * @param[in] request Segmented feature packet being acknowledged.
 * @param[in] transferred Assembled upload byte count.
 * @param[in] total_length Declared upload byte count.
 * @param[out] acknowledgement Thirteen-byte acknowledgement destination.
 * @return True when progress is valid and request and destination are usable; otherwise false.
 */
bool usb_feature_upload_acknowledgement_segmented_encode(
    uint8_t sequence, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE], uint16_t transferred,
    uint16_t total_length, uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE]);
/**
 * @brief Encodes a compact feature-upload acknowledgement.
 *
 * Echoes the report and transfer type and repeats the compact request length in the acknowledgement
 * progress fields.
 *
 * @param[in] sequence Vendor HID response sequence.
 * @param[in] request Compact feature packet being acknowledged.
 * @param[out] acknowledgement Thirteen-byte acknowledgement destination.
 * @return True when request and destination are usable; otherwise false.
 */
bool usb_feature_upload_acknowledgement_compact_encode(
    uint8_t sequence, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE]);

#endif
