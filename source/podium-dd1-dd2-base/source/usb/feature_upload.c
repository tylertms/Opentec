#include "usb/feature_upload.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    FEATURE_UPLOAD_HEADER_SIZE = 6,
    FEATURE_UPLOAD_PAYLOAD_SIZE = USB_FEATURE_UPLOAD_PACKET_SIZE - FEATURE_UPLOAD_HEADER_SIZE,
    FEATURE_UPLOAD_INITIAL = 0xf0,
    FEATURE_UPLOAD_TERMINAL = 0xa0,
    FEATURE_UPLOAD_MARKER = 0x80,
};

/**
 * @brief Decodes a segmented feature-upload offset.
 *
 * Combines the marked low seven bits with the upper field carried in 128-byte units.
 *
 * @param[in] low Marked low offset field.
 * @param[in] high Upper offset field.
 * @return Decoded transfer offset.
 */
static uint16_t decode_offset(uint8_t low, uint8_t high) {
    return (uint16_t)((uint16_t)high << 7) | (low & (FEATURE_UPLOAD_MARKER - 1u));
}

/**
 * @brief Initializes a segmented USB feature upload.
 *
 * Attaches the caller-owned assembly storage, selects the accepted report identifier, and resets
 * transfer progress.
 *
 * @param[out] upload Upload state to initialize.
 * @param[in] report_id Accepted feature report identifier.
 * @param[out] data Caller-owned upload assembly storage.
 * @param[in] capacity Available assembly byte count.
 * @return True when the state and assembly storage are usable.
 */
bool usb_feature_upload_init(UsbFeatureUpload *upload, uint8_t report_id, uint8_t *data,
                             uint16_t capacity) {
    if (upload == 0 || data == 0 || capacity == 0) {
        return false;
    }
    *upload = (UsbFeatureUpload){
        .data = data,
        .capacity = capacity,
        .report_id = report_id,
    };
    return true;
}

/**
 * @brief Accepts one segmented USB feature-upload packet.
 *
 * Reads the marked total length from the initial packet, assembles 58-byte continuation payloads
 * and the final short payload, requests acknowledgements after the initial and final data packets,
 * and completes after the empty terminal packet confirms the assembled length.
 *
 * @param[in,out] upload Active feature upload.
 * @param[in] packet Sixty-four-byte feature packet.
 * @param[in] length Received packet byte count.
 * @return Invalid input, continuation progress, acknowledgement request, or completed upload.
 */
UsbFeatureUploadEvent
usb_feature_upload_accept(UsbFeatureUpload *upload,
                          const uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE], uint8_t length) {
    UsbFeatureUploadEvent event = {.result = USB_FEATURE_UPLOAD_INVALID};
    if (upload == 0 || packet == 0 || upload->data == 0 || upload->offset > upload->total_length ||
        upload->complete || length != USB_FEATURE_UPLOAD_PACKET_SIZE ||
        packet[0] != upload->report_id) {
        return event;
    }
    upload->sequence = packet[2];

    if (!upload->active) {
        uint8_t count = packet[3];
        uint16_t total_length = decode_offset(packet[4], packet[5]);
        if (packet[1] != FEATURE_UPLOAD_INITIAL || (count & FEATURE_UPLOAD_MARKER) != 0 ||
            (packet[4] & FEATURE_UPLOAD_MARKER) == 0 || count > FEATURE_UPLOAD_PAYLOAD_SIZE ||
            count > total_length || total_length > upload->capacity) {
            return event;
        }
        memcpy(upload->data, &packet[FEATURE_UPLOAD_HEADER_SIZE], count);
        upload->total_length = total_length;
        upload->offset = count;
        upload->active = true;
        event.result = USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT;
        return event;
    }

    if (upload->offset < upload->total_length) {
        uint16_t remaining = upload->total_length - upload->offset;
        if (remaining > FEATURE_UPLOAD_PAYLOAD_SIZE) {
            if ((packet[3] & FEATURE_UPLOAD_MARKER) == 0 &&
                (packet[4] & FEATURE_UPLOAD_MARKER) == 0) {
                return event;
            }
            memcpy(upload->data + upload->offset, &packet[FEATURE_UPLOAD_HEADER_SIZE],
                   FEATURE_UPLOAD_PAYLOAD_SIZE);
            upload->offset += FEATURE_UPLOAD_PAYLOAD_SIZE;
            event.result = USB_FEATURE_UPLOAD_WAITING;
            return event;
        }

        uint8_t count = packet[3];
        if (count != remaining || count > FEATURE_UPLOAD_PAYLOAD_SIZE) {
            return event;
        }
        memcpy(upload->data + upload->offset, &packet[FEATURE_UPLOAD_HEADER_SIZE], count);
        upload->offset = upload->total_length;
        event.result = USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT;
        return event;
    }

    if (packet[1] != FEATURE_UPLOAD_TERMINAL ||
        decode_offset(packet[4], packet[5]) != upload->offset) {
        return event;
    }
    upload->complete = true;
    event.result = USB_FEATURE_UPLOAD_COMPLETE;
    event.data = upload->data;
    event.length = upload->total_length;
    return event;
}
