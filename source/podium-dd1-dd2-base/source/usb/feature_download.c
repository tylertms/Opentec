#include "usb/feature_download.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    FEATURE_DOWNLOAD_HEADER_SIZE = 6,
    FEATURE_DOWNLOAD_COMPACT_HEADER_SIZE = 4,
    FEATURE_DOWNLOAD_PAYLOAD_SIZE = USB_FEATURE_DOWNLOAD_PACKET_SIZE - FEATURE_DOWNLOAD_HEADER_SIZE,
    FEATURE_DOWNLOAD_INITIAL = 0xf0,
    FEATURE_DOWNLOAD_CONTINUE = 0xa0,
    FEATURE_DOWNLOAD_GROUP_END = 0xb0,
    FEATURE_DOWNLOAD_SINGLE = 0x30,
    FEATURE_DOWNLOAD_COMPACT_PROGRESS = 0xba,
    FEATURE_DOWNLOAD_COMPACT_PROGRESS_LIMIT = 0x7f,
    FEATURE_DOWNLOAD_OFFSET_LOW_MARKER = 0x80,
    FEATURE_DOWNLOAD_CONTINUATIONS_PER_ACKNOWLEDGEMENT = 5,
};

/**
 * @brief Encodes a segmented feature-download offset.
 *
 * Splits a 16-bit offset across the low and high fields while retaining the low-field marker.
 *
 * @param[in] offset Transfer offset to encode.
 * @param[out] low Encoded low field.
 * @param[out] high Encoded high field.
 */
static void encode_offset(uint16_t offset, uint8_t *low, uint8_t *high) {
    *low = (uint8_t)offset;
    if ((*low & FEATURE_DOWNLOAD_OFFSET_LOW_MARKER) != 0) {
        *high = (uint8_t)((offset >> 7) | 1u);
    } else {
        *low |= FEATURE_DOWNLOAD_OFFSET_LOW_MARKER;
        *high = (uint8_t)((offset >> 7) & 0xfeu);
    }
}

/**
 * @brief Encodes full-packet download progress.
 *
 * Uses the compact representation through 127 bytes and the marked offset representation above
 * that limit.
 *
 * @param[out] packet Feature packet whose progress fields are written.
 * @param[in] progress Total length or current transfer offset.
 */
static void encode_progress(uint8_t packet[USB_FEATURE_DOWNLOAD_PACKET_SIZE], uint16_t progress) {
    if (progress <= FEATURE_DOWNLOAD_COMPACT_PROGRESS_LIMIT) {
        packet[3] = FEATURE_DOWNLOAD_COMPACT_PROGRESS;
        packet[4] = 0;
        packet[5] = (uint8_t)progress;
    } else {
        packet[3] = FEATURE_DOWNLOAD_PAYLOAD_SIZE;
        encode_offset(progress, &packet[4], &packet[5]);
    }
}

/**
 * @brief Copies the next logical feature-download bytes.
 *
 * Inserts the optional leading zero without storing it in the caller-owned source and advances the
 * logical transfer offset by the requested count.
 *
 * @param[in,out] download Active feature download.
 * @param[in] data Caller-owned source bytes after the optional leading zero.
 * @param[out] destination Packet payload destination.
 * @param[in] count Logical byte count to copy.
 */
static void copy_payload(UsbFeatureDownload *download, const uint8_t *data, uint8_t *destination,
                         uint8_t count) {
    uint16_t prefix = download->leading_zero ? 1u : 0u;
    for (uint8_t index = 0; index < count; index++) {
        uint16_t position = download->offset + index;
        destination[index] = position < prefix ? 0 : data[position - prefix];
    }
    download->offset += count;
}

/**
 * @brief Initializes a segmented USB feature download.
 *
 * Selects the report identifier, sequence, logical transfer length, and optional leading zero, and
 * resets progress and acknowledgement cadence.
 *
 * @param[out] download Download state to initialize.
 * @param[in] report_id Feature report identifier.
 * @param[in] sequence Sequence carried by every transfer packet.
 * @param[in] total_length Logical transfer byte count including an optional leading zero.
 * @param[in] leading_zero Inserts a zero before the caller-owned source when true.
 */
void usb_feature_download_init(UsbFeatureDownload *download, uint8_t report_id, uint8_t sequence,
                               uint16_t total_length, bool leading_zero) {
    *download = (UsbFeatureDownload){
        .total_length = total_length,
        .report_id = report_id,
        .sequence = sequence,
        .leading_zero = leading_zero,
    };
}

/**
 * @brief Builds the next segmented USB feature packet.
 *
 * Emits compact single packets, 58-byte initial and continuation packets, acknowledgement group
 * boundaries every five continuations, final short packets, and the terminal empty packet.
 *
 * @param[in,out] download Active feature download.
 * @param[in] data Source bytes excluding the optional leading zero.
 * @param[out] packet Destination for the next feature packet.
 * @return Number of packet bytes produced, or zero while waiting or after completion.
 */
uint8_t usb_feature_download_next(UsbFeatureDownload *download, const uint8_t *data,
                                  uint8_t packet[USB_FEATURE_DOWNLOAD_PACKET_SIZE]) {
    uint16_t prefix = download != 0 && download->leading_zero ? 1u : 0u;
    if (download == 0 || packet == 0 || download->offset > download->total_length ||
        download->awaiting_acknowledgement || download->complete ||
        (download->total_length > prefix && data == 0)) {
        return 0;
    }

    packet[0] = download->report_id;
    packet[2] = download->sequence;
    uint16_t remaining = download->total_length - download->offset;

    if (download->offset == 0 && remaining <= FEATURE_DOWNLOAD_PAYLOAD_SIZE) {
        packet[1] = FEATURE_DOWNLOAD_SINGLE;
        packet[3] = (uint8_t)remaining;
        copy_payload(download, data, &packet[FEATURE_DOWNLOAD_COMPACT_HEADER_SIZE],
                     (uint8_t)remaining);
        download->awaiting_acknowledgement = true;
        return (uint8_t)(remaining + FEATURE_DOWNLOAD_COMPACT_HEADER_SIZE);
    }

    if (download->offset == 0) {
        packet[1] = FEATURE_DOWNLOAD_INITIAL;
        encode_progress(packet, download->total_length);
        copy_payload(download, data, &packet[FEATURE_DOWNLOAD_HEADER_SIZE],
                     FEATURE_DOWNLOAD_PAYLOAD_SIZE);
        download->awaiting_acknowledgement = true;
        return USB_FEATURE_DOWNLOAD_PACKET_SIZE;
    }

    if (remaining > FEATURE_DOWNLOAD_PAYLOAD_SIZE) {
        download->continuation_count++;
        download->awaiting_acknowledgement =
            download->continuation_count >= FEATURE_DOWNLOAD_CONTINUATIONS_PER_ACKNOWLEDGEMENT;
        packet[1] = download->awaiting_acknowledgement ? FEATURE_DOWNLOAD_GROUP_END
                                                       : FEATURE_DOWNLOAD_CONTINUE;
        if (download->awaiting_acknowledgement) {
            download->continuation_count = 0;
        }
        encode_progress(packet, download->offset);
        copy_payload(download, data, &packet[FEATURE_DOWNLOAD_HEADER_SIZE],
                     FEATURE_DOWNLOAD_PAYLOAD_SIZE);
        return USB_FEATURE_DOWNLOAD_PACKET_SIZE;
    }

    if (remaining != 0) {
        packet[1] = FEATURE_DOWNLOAD_GROUP_END;
        packet[3] = (uint8_t)remaining;
        encode_offset(download->offset, &packet[4], &packet[5]);
        copy_payload(download, data, &packet[FEATURE_DOWNLOAD_HEADER_SIZE], (uint8_t)remaining);
        download->awaiting_acknowledgement = true;
        return (uint8_t)(remaining + FEATURE_DOWNLOAD_HEADER_SIZE);
    }

    packet[1] = FEATURE_DOWNLOAD_CONTINUE;
    packet[3] = 0;
    if (download->total_length > FEATURE_DOWNLOAD_PAYLOAD_SIZE) {
        encode_offset(download->total_length, &packet[4], &packet[5]);
    } else {
        packet[4] = (uint8_t)download->total_length;
        packet[5] = 0;
    }
    download->complete = true;
    return FEATURE_DOWNLOAD_HEADER_SIZE;
}

/**
 * @brief Accepts a segmented feature-download acknowledgement.
 *
 * Matches status, response code, report identifier, transferred byte count, and remaining byte
 * count at the current acknowledgement boundary.
 *
 * @param[in,out] download Download waiting for acknowledgement.
 * @param[in] acknowledgement Thirteen-byte acknowledgement packet.
 * @return True when every acknowledgement field matches current transfer progress.
 */
bool usb_feature_download_acknowledge(
    UsbFeatureDownload *download,
    const uint8_t acknowledgement[USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE]) {
    if (download == 0 || acknowledgement == 0 || download->offset > download->total_length ||
        !download->awaiting_acknowledgement || acknowledgement[0] != 1 || acknowledgement[4] != 0 ||
        acknowledgement[5] != download->report_id) {
        return false;
    }

    uint32_t transferred = (uint32_t)acknowledgement[7] | (uint32_t)acknowledgement[8] << 8 |
                           (uint32_t)acknowledgement[9] << 16 | (uint32_t)acknowledgement[10] << 24;
    uint16_t remaining = (uint16_t)acknowledgement[11] | (uint16_t)acknowledgement[12] << 8;
    if (transferred != download->offset || remaining != download->total_length - download->offset) {
        return false;
    }

    download->awaiting_acknowledgement = false;
    return true;
}
