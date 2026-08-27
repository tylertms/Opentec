#include "shifter/frame.h"

#include <stdint.h>

enum {
    SHIFTER_CHECKSUM_OFFSET = 10,
    SHIFTER_CHECKSUM_INPUT_OFFSET = 1,
    SHIFTER_CHECKSUM_INPUT_SIZE = 9,
};

static uint16_t crc16_shift(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000u) != 0 ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint16_t shifter_checksum(const uint8_t *input) {
    uint16_t crc = 0;
    for (uint8_t index = 0; index < SHIFTER_CHECKSUM_INPUT_SIZE; index++) {
        crc = crc16_shift(crc, input[index]);
    }
    crc = crc16_shift(crc, 0);
    return crc16_shift(crc, 0);
}

static uint16_t read_u16(const uint8_t *input) { return input[0] | ((uint16_t)input[1] << 8); }

static void write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

void shifter_frame_encode(const ShifterFrame *frame, uint8_t output[SHIFTER_FRAME_SIZE]) {
    output[0] = SHIFTER_FRAME_START;
    output[1] = frame->command;
    for (uint8_t index = 0; index < SHIFTER_FRAME_PAYLOAD_SIZE; index++) {
        output[index + 2] = frame->payload[index];
    }
    write_u16(output + 6, frame->primary_position);
    write_u16(output + 8, frame->secondary_position);
    write_u16(output + SHIFTER_CHECKSUM_OFFSET,
              shifter_checksum(output + SHIFTER_CHECKSUM_INPUT_OFFSET));
    output[12] = SHIFTER_FRAME_END;
}

ShifterFrameResult shifter_frame_decode(const uint8_t input[SHIFTER_FRAME_SIZE],
                                        ShifterFrame *frame) {
    if (input[0] != SHIFTER_FRAME_START || input[12] != SHIFTER_FRAME_END) {
        return SHIFTER_FRAME_INVALID_BOUNDARY;
    }

    uint16_t expected = shifter_checksum(input + SHIFTER_CHECKSUM_INPUT_OFFSET);
    if (read_u16(input + SHIFTER_CHECKSUM_OFFSET) != expected) {
        return SHIFTER_FRAME_INVALID_CHECKSUM;
    }

    frame->command = input[1];
    for (uint8_t index = 0; index < SHIFTER_FRAME_PAYLOAD_SIZE; index++) {
        frame->payload[index] = input[index + 2];
    }
    frame->primary_position = read_u16(input + 6);
    frame->secondary_position = read_u16(input + 8);
    return SHIFTER_FRAME_VALID;
}
