#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_mode_one.h"

static void test_identifies_shared_codec_modes(void) {
    assert(wheel_packet_mode_one_applies(1));
    assert(wheel_packet_mode_one_applies(3));
    assert(wheel_packet_mode_one_applies(0x13));
    assert(wheel_packet_mode_one_applies(0x14));
    assert(!wheel_packet_mode_one_applies(0));
    assert(!wheel_packet_mode_one_applies(2));
    assert(!wheel_packet_mode_one_applies(0x12));
    assert(!wheel_packet_mode_one_applies(0x15));
}

static void test_encodes_the_complete_response(void) {
    const WheelPacketModeOneOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .third_glyph_marker = true},
        .operating_mode = 0x12,
        .display_state = {0x44, 0x55},
        .link_status = {0x66, 0x77},
    };
    uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {0};
    const uint8_t expected[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {
        0xa5, 0x00, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };

    wheel_packet_mode_one_encode(&output, response);
    assert(memcmp(response, expected, sizeof(expected)) == 0);
}

static void test_requests_authentication_for_operating_modes_0x13_and_0x14(void) {
    WheelPacketModeOneOutput output = {0};
    uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {0};

    output.operating_mode = 0x13;
    wheel_packet_mode_one_encode(&output, response);
    assert(response[0] == 0xa6);

    output.operating_mode = 0x14;
    wheel_packet_mode_one_encode(&output, response);
    assert(response[0] == 0xa6);

    output.operating_mode = 0x15;
    wheel_packet_mode_one_encode(&output, response);
    assert(response[0] == 0xa5);
}

int main(void) {
    test_identifies_shared_codec_modes();
    test_encodes_the_complete_response();
    test_requests_authentication_for_operating_modes_0x13_and_0x14();
    return 0;
}
