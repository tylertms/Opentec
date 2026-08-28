#include "pedal/frame.h"

#include <stdint.h>

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
 * Encodes a pedal protocol message into its 12-byte framed representation.
 *
 * @param frame Message type and eight-byte payload to encode.
 * @param output Destination for the framed message.
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
 * Validates and decodes a 12-byte pedal protocol message.
 *
 * @param input Complete framed message.
 * @param frame Destination for the decoded message type and payload.
 * @return Frame status identifying boundary or checksum failures.
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
