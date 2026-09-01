#include "usb/feature_upload_acknowledgement.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_upload.h"

/** @brief Feature-upload acknowledgement envelope constants. */
enum {
    FEATURE_UPLOAD_ACKNOWLEDGEMENT_STATUS = 1,     /**< Successful acknowledgement status. */
    FEATURE_UPLOAD_ACKNOWLEDGEMENT_COMMAND = 0x20, /**< Feature-upload acknowledgement command. */
    FEATURE_UPLOAD_ACKNOWLEDGEMENT_PAYLOAD_LENGTH = 9, /**< Acknowledgement payload length. */
};

/**
 * @brief Initializes a feature-upload acknowledgement envelope.
 *
 * Writes the success status, vendor command, sequence, payload length, response code, and echoed
 * report and transfer type fields shared by compact and segmented acknowledgements.
 *
 * @param[in] sequence Vendor HID response sequence.
 * @param[in] request Feature upload packet being acknowledged.
 * @param[out] acknowledgement Thirteen-byte acknowledgement destination.
 * @return True when the request and destination are usable.
 */
static bool initialize(uint8_t sequence, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
                       uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE]) {
    if (request == 0 || acknowledgement == 0) {
        return false;
    }
    acknowledgement[0] = FEATURE_UPLOAD_ACKNOWLEDGEMENT_STATUS;
    acknowledgement[1] = FEATURE_UPLOAD_ACKNOWLEDGEMENT_COMMAND;
    acknowledgement[2] = sequence;
    acknowledgement[3] = FEATURE_UPLOAD_ACKNOWLEDGEMENT_PAYLOAD_LENGTH;
    acknowledgement[4] = 0;
    acknowledgement[5] = request[0];
    acknowledgement[6] = request[1];
    return true;
}

bool usb_feature_upload_acknowledgement_segmented_encode(
    uint8_t sequence, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE], uint16_t transferred,
    uint16_t total_length, uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE]) {
    if (transferred > total_length || !initialize(sequence, request, acknowledgement)) {
        return false;
    }
    acknowledgement[7] = (uint8_t)transferred;
    acknowledgement[8] = (uint8_t)(transferred >> 8);
    acknowledgement[9] = 0;
    acknowledgement[10] = 0;
    uint16_t remaining = total_length - transferred;
    acknowledgement[11] = (uint8_t)remaining;
    acknowledgement[12] = (uint8_t)(remaining >> 8);
    return true;
}

bool usb_feature_upload_acknowledgement_compact_encode(
    uint8_t sequence, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE]) {
    if (!initialize(sequence, request, acknowledgement)) {
        return false;
    }
    acknowledgement[7] = request[3];
    acknowledgement[8] = 0;
    acknowledgement[9] = 0;
    acknowledgement[10] = 0;
    acknowledgement[11] = request[3];
    acknowledgement[12] = 0;
    return true;
}
