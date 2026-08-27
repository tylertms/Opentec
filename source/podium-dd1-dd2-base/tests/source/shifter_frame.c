#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "shifter/frame.h"

static const uint8_t position_frame[SHIFTER_FRAME_SIZE] = {
    0x7b, 0x01, 0x05, 0x06, 0x07, 0x08, 0xcd, 0xab, 0x68, 0x24, 0xfb, 0xa4, 0x7d,
};

static void test_decode_position(void) {
    ShifterFrame frame;

    assert(shifter_frame_decode(position_frame, &frame) == SHIFTER_FRAME_VALID);
    assert(frame.command == SHIFTER_COMMAND_POSITION);
    assert(frame.payload[0] == 0x05);
    assert(frame.payload[1] == 0x06);
    assert(frame.payload[2] == 0x07);
    assert(frame.payload[3] == 0x08);
    assert(frame.primary_position == 0xabcd);
    assert(frame.secondary_position == 0x2468);
}

static void test_encode_position(void) {
    const ShifterFrame frame = {
        .command = SHIFTER_COMMAND_POSITION,
        .payload = {0x05, 0x06, 0x07, 0x08},
        .primary_position = 0xabcd,
        .secondary_position = 0x2468,
    };
    uint8_t encoded[SHIFTER_FRAME_SIZE];

    shifter_frame_encode(&frame, encoded);
    assert(memcmp(encoded, position_frame, sizeof(encoded)) == 0);
}

static void test_replay_checksum(void) {
    const ShifterFrame frame = {
        .command = SHIFTER_COMMAND_REPLAY,
        .payload = {0xde, 0xad, 0xbe, 0xef},
        .primary_position = 0x2468,
        .secondary_position = 0xace0,
    };
    const uint8_t expected[SHIFTER_FRAME_SIZE] = {
        0x7b, 0x81, 0xde, 0xad, 0xbe, 0xef, 0x68, 0x24, 0xe0, 0xac, 0x6b, 0x04, 0x7d,
    };
    uint8_t encoded[SHIFTER_FRAME_SIZE];

    shifter_frame_encode(&frame, encoded);
    assert(memcmp(encoded, expected, sizeof(encoded)) == 0);
}

static void test_rejects_invalid_frames(void) {
    uint8_t input[SHIFTER_FRAME_SIZE];
    ShifterFrame frame = {0};

    memcpy(input, position_frame, sizeof(input));
    input[0] = 0;
    assert(shifter_frame_decode(input, &frame) == SHIFTER_FRAME_INVALID_BOUNDARY);

    memcpy(input, position_frame, sizeof(input));
    input[12] = 0;
    assert(shifter_frame_decode(input, &frame) == SHIFTER_FRAME_INVALID_BOUNDARY);

    memcpy(input, position_frame, sizeof(input));
    input[2] ^= 1;
    assert(shifter_frame_decode(input, &frame) == SHIFTER_FRAME_INVALID_CHECKSUM);
}

int main(void) {
    test_decode_position();
    test_encode_position();
    test_replay_checksum();
    test_rejects_invalid_frames();
    return 0;
}
