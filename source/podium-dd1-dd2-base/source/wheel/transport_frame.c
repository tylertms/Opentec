#include "wheel/transport_frame.h"

#include <stdint.h>

enum {
    WHEEL_TRANSPORT_BODY_OFFSET = 1,
    WHEEL_TRANSPORT_BODY_SIZE = 60,
    WHEEL_TRANSPORT_DATA_OFFSET = 4,
    WHEEL_TRANSPORT_CHECKSUM_OFFSET = 61,
};

static uint16_t update_checksum(uint16_t checksum, uint8_t data) {
    uint16_t mixed = (uint8_t)data ^ checksum;
    uint16_t low = (uint8_t)mixed;
    uint16_t nibble = mixed & 0x0f;
    mixed = low ^ (nibble << 4);
    uint16_t high = mixed >> 4;
    uint16_t result = (checksum >> 8) ^ high;
    return result ^ ((((mixed + mixed) ^ high) << 4) ^ nibble) << 3;
}

static uint16_t checksum(const uint8_t *body) {
    uint16_t value = 0;
    for (uint8_t index = 0; index < WHEEL_TRANSPORT_BODY_SIZE; index++) {
        value = update_checksum(value, body[index]);
    }
    return value;
}

WheelTransportFrameResult wheel_transport_frame_encode(const WheelTransportFrame *frame,
                                                       uint8_t output[WHEEL_TRANSPORT_FRAME_SIZE]) {
    if (frame->length > WHEEL_TRANSPORT_PAYLOAD_SIZE) {
        return WHEEL_TRANSPORT_FRAME_INVALID_LENGTH;
    }

    for (uint8_t index = 0; index < WHEEL_TRANSPORT_FRAME_SIZE; index++) {
        output[index] = 0;
    }
    output[0] = WHEEL_TRANSPORT_FRAME_START;
    output[1] = frame->command;
    output[2] = frame->node;
    output[3] = frame->length;
    for (uint8_t index = 0; index < frame->length; index++) {
        output[WHEEL_TRANSPORT_DATA_OFFSET + index] = frame->data[index];
    }
    uint16_t value = checksum(output + WHEEL_TRANSPORT_BODY_OFFSET);
    output[WHEEL_TRANSPORT_CHECKSUM_OFFSET] = (uint8_t)value;
    output[WHEEL_TRANSPORT_CHECKSUM_OFFSET + 1] = (uint8_t)(value >> 8);
    output[WHEEL_TRANSPORT_FRAME_SIZE - 1] = WHEEL_TRANSPORT_FRAME_END;
    return WHEEL_TRANSPORT_FRAME_VALID;
}

WheelTransportFrameResult
wheel_transport_frame_decode(const uint8_t input[WHEEL_TRANSPORT_FRAME_SIZE],
                             WheelTransportFrame *frame) {
    if (input[0] != WHEEL_TRANSPORT_FRAME_START ||
        input[WHEEL_TRANSPORT_FRAME_SIZE - 1] != WHEEL_TRANSPORT_FRAME_END) {
        return WHEEL_TRANSPORT_FRAME_INVALID_BOUNDARY;
    }
    if (input[3] > WHEEL_TRANSPORT_PAYLOAD_SIZE) {
        return WHEEL_TRANSPORT_FRAME_INVALID_LENGTH;
    }
    uint16_t expected = (uint16_t)input[WHEEL_TRANSPORT_CHECKSUM_OFFSET] |
                        (uint16_t)input[WHEEL_TRANSPORT_CHECKSUM_OFFSET + 1] << 8;
    if (checksum(input + WHEEL_TRANSPORT_BODY_OFFSET) != expected) {
        return WHEEL_TRANSPORT_FRAME_INVALID_CHECKSUM;
    }

    frame->command = input[1];
    frame->node = input[2];
    frame->length = input[3];
    for (uint8_t index = 0; index < WHEEL_TRANSPORT_PAYLOAD_SIZE; index++) {
        frame->data[index] = input[WHEEL_TRANSPORT_DATA_OFFSET + index];
    }
    return WHEEL_TRANSPORT_FRAME_VALID;
}
