#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pedal/frame.h"

static void test_handshake_frame(void) {
    const PedalFrame frame = {
        .type = 2,
        .payload = {0xff, 0, 0, 0, 0, 0, 0, 0},
    };
    const uint8_t expected[PEDAL_FRAME_SIZE] = {
        0x7b, 0x02, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x7d,
    };
    uint8_t encoded[PEDAL_FRAME_SIZE];
    PedalFrame decoded;

    pedal_frame_encode(&frame, encoded);
    assert(memcmp(encoded, expected, sizeof(encoded)) == 0);
    assert(pedal_frame_decode(encoded, &decoded) == PEDAL_FRAME_VALID);
    assert(decoded.type == frame.type);
    assert(memcmp(decoded.payload, frame.payload, sizeof(decoded.payload)) == 0);
}

static void test_input_frame(void) {
    const PedalFrame frame = {
        .type = 3,
        .payload = {1, 2, 3, 0, 0, 0, 0, 0},
    };
    const uint8_t expected[PEDAL_FRAME_SIZE] = {
        0x7b, 0x03, 0x01, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x7d,
    };
    uint8_t encoded[PEDAL_FRAME_SIZE];

    pedal_frame_encode(&frame, encoded);
    assert(memcmp(encoded, expected, sizeof(encoded)) == 0);
}

static void test_rejects_invalid_frames(void) {
    const PedalFrame frame = {
        .type = 0x10,
        .payload = {0},
    };
    uint8_t input[PEDAL_FRAME_SIZE];
    PedalFrame decoded = {0};

    pedal_frame_encode(&frame, input);
    input[0] = 0;
    assert(pedal_frame_decode(input, &decoded) == PEDAL_FRAME_INVALID_BOUNDARY);

    pedal_frame_encode(&frame, input);
    input[11] = 0;
    assert(pedal_frame_decode(input, &decoded) == PEDAL_FRAME_INVALID_BOUNDARY);

    pedal_frame_encode(&frame, input);
    input[2] ^= 1;
    assert(pedal_frame_decode(input, &decoded) == PEDAL_FRAME_INVALID_CHECKSUM);
}

int main(void) {
    test_handshake_frame();
    test_input_frame();
    test_rejects_invalid_frames();
    return 0;
}
