#include "pedal/frame.h"

#include <stdint.h>

/**
 * @brief Computes the checksum for a pedal frame.
 *
 * Applies the reflected CRC-8 polynomial 0x8c with an initial value of 0xff to the frame type and
 * eight payload bytes.
 *
 * @param[in] input Frame type followed by the eight-byte payload.
 * @return The eight-bit frame checksum.
 */
static uint8_t pedal_checksum(const uint8_t *input) {
    uint8_t crc = UINT8_MAX;
    for (uint8_t index = 0; index < PEDAL_FRAME_PAYLOAD_SIZE + 1; index++) {
        crc ^= input[index];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) != 0 ? (uint8_t)((crc >> 1) ^ 0x8cu) : (uint8_t)(crc >> 1);
        }
    }
    return crc;
}

/**
 * @brief Encodes a pedal protocol message.
 *
 * Writes the start marker, frame type, eight payload bytes, checksum, and end marker in their wire
 * order. The checksum covers only the type and payload.
 *
 * @param[in] frame Message type and eight-byte payload to encode.
 * @param[out] output Destination for the twelve-byte framed message.
 */
void pedal_frame_encode(const PedalFrame *frame, uint8_t output[PEDAL_FRAME_SIZE]) {
    output[0] = PEDAL_FRAME_START;
    output[1] = frame->type;
    for (uint8_t index = 0; index < PEDAL_FRAME_PAYLOAD_SIZE; index++) {
        output[index + 2] = frame->payload[index];
    }
    output[10] = pedal_checksum(output + 1);
    output[11] = PEDAL_FRAME_END;
}

/**
 * @brief Validates and decodes a pedal protocol message.
 *
 * Checks the start and end markers before the checksum. The decoded type and payload are written
 * only after both checks pass.
 *
 * @param[in] input Complete twelve-byte framed message.
 * @param[out] frame Destination for the decoded message type and payload.
 * @return The frame status identifying boundary or checksum failures.
 */
PedalFrameResult pedal_frame_decode(const uint8_t input[PEDAL_FRAME_SIZE], PedalFrame *frame) {
    if (input[0] != PEDAL_FRAME_START || input[11] != PEDAL_FRAME_END) {
        return PEDAL_FRAME_INVALID_BOUNDARY;
    }
    if (input[10] != pedal_checksum(input + 1)) {
        return PEDAL_FRAME_INVALID_CHECKSUM;
    }

    frame->type = input[1];
    for (uint8_t index = 0; index < PEDAL_FRAME_PAYLOAD_SIZE; index++) {
        frame->payload[index] = input[index + 2];
    }
    return PEDAL_FRAME_VALID;
}
