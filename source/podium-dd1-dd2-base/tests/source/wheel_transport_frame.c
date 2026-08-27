#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/transport_frame.h"

static void test_encodes_known_button_request(void) {
    const WheelTransportFrame frame = {
        .command = 3,
        .node = 0,
        .length = 3,
        .data = {1, 2, 3},
    };
    uint8_t encoded[WHEEL_TRANSPORT_FRAME_SIZE];
    WheelTransportFrame decoded;

    assert(wheel_transport_frame_encode(&frame, encoded) == WHEEL_TRANSPORT_FRAME_VALID);
    assert(encoded[0] == WHEEL_TRANSPORT_FRAME_START);
    assert(encoded[61] == 0xa9);
    assert(encoded[62] == 0x39);
    assert(encoded[63] == WHEEL_TRANSPORT_FRAME_END);
    assert(wheel_transport_frame_decode(encoded, &decoded) == WHEEL_TRANSPORT_FRAME_VALID);
    assert(decoded.command == frame.command);
    assert(decoded.node == frame.node);
    assert(decoded.length == frame.length);
    assert(memcmp(decoded.data, frame.data, frame.length) == 0);
}

static void test_rejects_invalid_frames(void) {
    const WheelTransportFrame frame = {
        .command = 5,
        .node = 7,
        .length = 1,
        .data = {9},
    };
    uint8_t encoded[WHEEL_TRANSPORT_FRAME_SIZE];
    WheelTransportFrame decoded;
    assert(wheel_transport_frame_encode(&frame, encoded) == WHEEL_TRANSPORT_FRAME_VALID);

    encoded[0] = 0;
    assert(wheel_transport_frame_decode(encoded, &decoded) ==
           WHEEL_TRANSPORT_FRAME_INVALID_BOUNDARY);
    assert(wheel_transport_frame_encode(&frame, encoded) == WHEEL_TRANSPORT_FRAME_VALID);
    encoded[3] = WHEEL_TRANSPORT_PAYLOAD_SIZE + 1;
    assert(wheel_transport_frame_decode(encoded, &decoded) == WHEEL_TRANSPORT_FRAME_INVALID_LENGTH);
    assert(wheel_transport_frame_encode(&frame, encoded) == WHEEL_TRANSPORT_FRAME_VALID);
    encoded[4] ^= 1;
    assert(wheel_transport_frame_decode(encoded, &decoded) ==
           WHEEL_TRANSPORT_FRAME_INVALID_CHECKSUM);
}

static void test_rejects_oversized_payload(void) {
    WheelTransportFrame frame = {
        .length = WHEEL_TRANSPORT_PAYLOAD_SIZE + 1,
    };
    uint8_t encoded[WHEEL_TRANSPORT_FRAME_SIZE];
    assert(wheel_transport_frame_encode(&frame, encoded) == WHEEL_TRANSPORT_FRAME_INVALID_LENGTH);
}

int main(void) {
    test_encodes_known_button_request();
    test_rejects_invalid_frames();
    test_rejects_oversized_payload();
    return 0;
}
