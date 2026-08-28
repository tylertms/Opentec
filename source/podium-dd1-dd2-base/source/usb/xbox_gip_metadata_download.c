#include "usb/xbox_gip_metadata_download.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    METADATA_PACKET_HEADER_SIZE = 6,
    METADATA_PACKET_PAYLOAD_SIZE = USB_XBOX_GIP_METADATA_PACKET_SIZE - METADATA_PACKET_HEADER_SIZE,
    METADATA_TRANSFER_CONTINUE = 0xa0,
    METADATA_TRANSFER_GROUP_END = 0xb0,
    METADATA_TRANSFER_INITIAL = 0xf0,
    METADATA_COMPACT_PROGRESS_LIMIT = 0x7f,
    METADATA_OFFSET_LOW_MARKER = 0x80,
    METADATA_CONTINUATIONS_PER_ACKNOWLEDGEMENT = 5,
};

/**
 * @brief Encodes a metadata transfer offset.
 *
 * Splits a 16-bit offset across the low and high packet fields while preserving the low-field
 * continuation marker.
 *
 * @param[in] offset Transfer offset to encode.
 * @param[out] low Encoded low field.
 * @param[out] high Encoded high field.
 */
static void encode_offset(uint16_t offset, uint8_t *low, uint8_t *high) {
    *low = (uint8_t)offset;
    if ((*low & METADATA_OFFSET_LOW_MARKER) != 0) {
        *high = (uint8_t)((offset >> 7) | 1u);
    } else {
        *low |= METADATA_OFFSET_LOW_MARKER;
        *high = (uint8_t)((offset >> 7) & 0xfeu);
    }
}

/**
 * @brief Encodes the progress fields of a full metadata packet.
 *
 * Uses the compact form through 127 bytes and the marked two-byte offset form above that limit.
 *
 * @param[out] packet Metadata packet whose progress fields are written.
 * @param[in] progress Document length or transfer offset to encode.
 */
static void encode_progress(uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE], uint16_t progress) {
    if (progress <= METADATA_COMPACT_PROGRESS_LIMIT) {
        packet[3] = 0xba;
        packet[4] = 0;
        packet[5] = (uint8_t)progress;
        return;
    }

    packet[3] = 0x3a;
    encode_offset(progress, &packet[4], &packet[5]);
}

/**
 * @brief Starts an Xbox GIP metadata download.
 *
 * Resets the document offset and continuation cadence while retaining the sequence selected for
 * every packet in the exchange.
 *
 * @param[out] download Download state to initialize.
 * @param[in] sequence Sequence value carried by the metadata packets.
 */
void usb_xbox_gip_metadata_download_init(UsbXboxGipMetadataDownload *download, uint8_t sequence) {
    *download = (UsbXboxGipMetadataDownload){.sequence = sequence};
}

/**
 * @brief Builds the next Xbox GIP metadata packet.
 *
 * Emits the initial 58-byte packet, continuation groups, the final short packet, and the terminal
 * six-byte packet. Packet production pauses at each required acknowledgement boundary.
 *
 * @param[in,out] download Active metadata download state.
 * @param[in] metadata Encoded 449-byte metadata document.
 * @param[out] packet Destination for the next packet.
 * @param[out] packet_length Number of bytes written to the packet.
 * @return True when a packet is produced; false while waiting or after completion.
 */
bool usb_xbox_gip_metadata_download_next(UsbXboxGipMetadataDownload *download,
                                         const uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE],
                                         uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE],
                                         uint8_t *packet_length) {
    *packet_length = 0;
    if (download->awaiting_acknowledgement || download->complete) {
        return false;
    }

    packet[0] = USB_XBOX_GIP_METADATA_REPORT_ID;
    packet[2] = download->sequence;

    if (download->offset == 0) {
        packet[1] = METADATA_TRANSFER_INITIAL;
        encode_progress(packet, USB_XBOX_GIP_METADATA_SIZE);
        memcpy(&packet[METADATA_PACKET_HEADER_SIZE], metadata, METADATA_PACKET_PAYLOAD_SIZE);
        download->offset = METADATA_PACKET_PAYLOAD_SIZE;
        download->awaiting_acknowledgement = true;
        *packet_length = USB_XBOX_GIP_METADATA_PACKET_SIZE;
        return true;
    }

    uint16_t remaining = USB_XBOX_GIP_METADATA_SIZE - download->offset;
    if (remaining > METADATA_PACKET_PAYLOAD_SIZE) {
        download->continuation_count++;
        bool acknowledgement_due =
            download->continuation_count >= METADATA_CONTINUATIONS_PER_ACKNOWLEDGEMENT;
        packet[1] = acknowledgement_due ? METADATA_TRANSFER_GROUP_END : METADATA_TRANSFER_CONTINUE;
        encode_progress(packet, download->offset);
        memcpy(&packet[METADATA_PACKET_HEADER_SIZE], &metadata[download->offset],
               METADATA_PACKET_PAYLOAD_SIZE);
        download->offset += METADATA_PACKET_PAYLOAD_SIZE;
        download->awaiting_acknowledgement = acknowledgement_due;
        if (acknowledgement_due) {
            download->continuation_count = 0;
        }
        *packet_length = USB_XBOX_GIP_METADATA_PACKET_SIZE;
        return true;
    }

    if (remaining != 0) {
        packet[1] = METADATA_TRANSFER_GROUP_END;
        packet[3] = (uint8_t)remaining;
        encode_offset(download->offset, &packet[4], &packet[5]);
        memcpy(&packet[METADATA_PACKET_HEADER_SIZE], &metadata[download->offset], remaining);
        download->offset += remaining;
        download->awaiting_acknowledgement = true;
        *packet_length = (uint8_t)(remaining + METADATA_PACKET_HEADER_SIZE);
        return true;
    }

    packet[1] = METADATA_TRANSFER_CONTINUE;
    packet[3] = 0;
    encode_offset(USB_XBOX_GIP_METADATA_SIZE, &packet[4], &packet[5]);
    download->complete = true;
    *packet_length = METADATA_PACKET_HEADER_SIZE;
    return true;
}

/**
 * @brief Accepts an Xbox GIP metadata transfer acknowledgement.
 *
 * Matches the report identifier, status, transferred byte count, and remaining byte count at the
 * current acknowledgement boundary.
 *
 * @param[in,out] download Metadata download waiting for acknowledgement.
 * @param[in] acknowledgement Thirteen-byte acknowledgement packet.
 * @return True when every acknowledgement field matches the current transfer state.
 */
bool usb_xbox_gip_metadata_download_acknowledge(
    UsbXboxGipMetadataDownload *download,
    const uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE]) {
    if (!download->awaiting_acknowledgement || acknowledgement[0] != 1 || acknowledgement[4] != 0 ||
        acknowledgement[5] != USB_XBOX_GIP_METADATA_REPORT_ID) {
        return false;
    }

    uint32_t transferred = (uint32_t)acknowledgement[7] | (uint32_t)acknowledgement[8] << 8 |
                           (uint32_t)acknowledgement[9] << 16 | (uint32_t)acknowledgement[10] << 24;
    uint16_t remaining = (uint16_t)acknowledgement[11] | (uint16_t)acknowledgement[12] << 8;
    if (transferred != download->offset ||
        remaining != USB_XBOX_GIP_METADATA_SIZE - download->offset) {
        return false;
    }

    download->awaiting_acknowledgement = false;
    return true;
}
