#include "wqr_frame.h"

#include <string.h>

enum {
    FRAME_START = 0x7b,
    FRAME_END = 0x7d,
    FRAME_TYPE_OFFSET = 1,
    FRAME_SEQUENCE_OFFSET = 2,
    FRAME_LENGTH_OFFSET = 3,
    FRAME_PAYLOAD_OFFSET = 4,
    FRAME_CRC_OFFSET = 61,
    FRAME_END_OFFSET = 63
};

/**
 * @brief Reads one little-endian 16-bit frame field.
 *
 * Combines the two bytes beginning at the supplied address into an unsigned value.
 *
 * @param[in] data First byte of the field.
 * @return Decoded unsigned value.
 */
static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

/**
 * @brief Writes one little-endian 16-bit frame field.
 *
 * Stores the low byte first at the supplied address.
 *
 * @param[out] data First byte of the destination field.
 * @param[in] value Unsigned value to encode.
 */
static void write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Calculates the WQR frame CRC.
 *
 * Applies the reflected CRC-16 polynomial `0x8408` with an initial value of zero across the
 * requested byte range.
 *
 * @param[in] data Bytes to include in the calculation.
 * @param[in] length Number of bytes to process.
 * @return Calculated 16-bit CRC.
 */
uint16_t wqr_frame_crc(const uint8_t *data, size_t length) {
    uint16_t crc = 0;

    while (length-- != 0) {
        unsigned int bit;

        crc ^= *data++;
        for (bit = 0; bit < 8; ++bit) {
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1) != 0 ? 0x8408 : 0));
        }
    }

    return crc;
}

/**
 * @brief Builds one complete WQR transport frame.
 *
 * Clears the destination, writes framing metadata and payload, and appends the CRC and end marker.
 * Rejects null destinations, oversized payloads, and null nonempty payloads.
 *
 * @param[out] frame Complete 64-byte destination frame.
 * @param[in] type_flags Payload type and fragmentation flags.
 * @param[in] sequence Frame sequence value.
 * @param[in] payload Payload bytes, or null when the payload length is zero.
 * @param[in] payload_length Number of payload bytes to encode.
 * @return True when a valid frame was built.
 */
bool wqr_frame_build(uint8_t frame[WQR_FRAME_SIZE], uint8_t type_flags, uint8_t sequence,
                     const uint8_t *payload, size_t payload_length) {
    uint16_t crc;

    if (frame == NULL || payload_length > WQR_FRAME_PAYLOAD_SIZE ||
        (payload == NULL && payload_length != 0)) {
        return false;
    }

    memset(frame, 0, WQR_FRAME_SIZE);
    frame[0] = FRAME_START;
    frame[FRAME_TYPE_OFFSET] = type_flags;
    frame[FRAME_SEQUENCE_OFFSET] = sequence;
    frame[FRAME_LENGTH_OFFSET] = (uint8_t)payload_length;
    if (payload_length != 0) {
        memcpy(frame + FRAME_PAYLOAD_OFFSET, payload, payload_length);
    }

    crc = wqr_frame_crc(frame + FRAME_TYPE_OFFSET, WQR_FRAME_BODY_SIZE);
    write_u16(frame + FRAME_CRC_OFFSET, crc);
    frame[FRAME_END_OFFSET] = FRAME_END;
    return true;
}

/**
 * @brief Validates and decodes one complete WQR transport frame.
 *
 * Verifies boundary markers, payload length, and CRC before publishing a view into the supplied
 * frame buffer.
 *
 * @param[in] frame Complete 64-byte source frame.
 * @param[out] view Decoded metadata and payload view.
 * @return True when the frame is structurally valid and has a matching CRC.
 */
bool wqr_frame_parse(const uint8_t frame[WQR_FRAME_SIZE], wqr_frame_view *view) {
    if (frame == NULL || view == NULL || frame[0] != FRAME_START ||
        frame[FRAME_END_OFFSET] != FRAME_END ||
        frame[FRAME_LENGTH_OFFSET] > WQR_FRAME_PAYLOAD_SIZE ||
        read_u16(frame + FRAME_CRC_OFFSET) !=
            wqr_frame_crc(frame + FRAME_TYPE_OFFSET, WQR_FRAME_BODY_SIZE)) {
        return false;
    }

    view->type_flags = frame[FRAME_TYPE_OFFSET];
    view->sequence = frame[FRAME_SEQUENCE_OFFSET];
    view->payload = frame + FRAME_PAYLOAD_OFFSET;
    view->payload_length = frame[FRAME_LENGTH_OFFSET];
    return true;
}
