#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_packed.h"

static void test_identifies_packed_modes(void) {
    assert(wheel_packet_packed_applies(0x0f));
    assert(wheel_packet_packed_applies(0x17));
    assert(!wheel_packet_packed_applies(0x0e));
    assert(!wheel_packet_packed_applies(0x10));
    assert(!wheel_packet_packed_applies(0x16));
    assert(!wheel_packet_packed_applies(0x18));
}

static void test_decodes_packed_input_fields(void) {
    uint8_t request[WHEEL_PACKET_PACKED_REQUEST_SIZE] = {0};
    const uint8_t payload[WHEEL_PACKET_PACKED_SNAPSHOT_SIZE] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0xfe, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x34, 0xab, 0x70,
        0x71, 0x78, 0x56, 0x34, 0x12, 0x72, 0x73, 0xa0, 0xa1, 0xa2, 0xa3, 0x74, 0xa4, 0x75, 0x76,
    };
    memcpy(&request[2], payload, sizeof(payload));

    WheelPacketPackedInput input;
    wheel_packet_packed_decode(request, &input);

    assert(input.buttons[0] == 0x11);
    assert(input.buttons[1] == 0x22);
    assert(input.buttons[2] == 0x33);
    assert(input.axis_outputs[0] == 0x44);
    assert(input.axis_outputs[1] == 0x55);
    assert(input.motion == -2);
    assert(input.controls[0] == 0x61);
    assert(input.controls[6] == 0x34);
    assert(input.controls[7] == 0xab);
    assert(input.reserved_axes[0] == 0x70);
    assert(input.reserved_axes[1] == 0x71);
    assert(input.axis_values[0] == 0x5678);
    assert(input.axis_values[1] == 0x1234);
    assert(input.mode_buttons == 0x72);
    assert(input.axis_report_enabled == 0x73);
    assert(input.auxiliary_data[0] == 0xa0);
    assert(input.auxiliary_data[3] == 0xa3);
    assert(input.report_mode == 0x74);
    assert(input.reserved_report == 0xa4);
    assert(input.report_capabilities == 0x75);
    assert(input.axis_limit == 0x76);
}

static void test_filters_buttons_across_three_samples(void) {
    WheelPacketPackedFilter filter;
    wheel_packet_packed_filter_init(&filter);

    WheelPacketPackedInput input = {.buttons = {0xf3, 0x5a, 0xff}};
    wheel_packet_packed_filter_buttons(&filter, &input);
    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0);
    assert(input.buttons[2] == 0);

    input = (WheelPacketPackedInput){.buttons = {0xf7, 0x7a, 0x7f}};
    wheel_packet_packed_filter_buttons(&filter, &input);
    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0);
    assert(input.buttons[2] == 0);

    input = (WheelPacketPackedInput){.buttons = {0xfb, 0x5e, 0xff}};
    wheel_packet_packed_filter_buttons(&filter, &input);
    assert(input.buttons[0] == 0xf3);
    assert(input.buttons[1] == 0x5a);
    assert(input.buttons[2] == 0x7f);
    assert(filter.next_sample == 0);
}

static void test_normalizes_buttons_and_packed_axes(void) {
    WheelPacketPackedInput input = {
        .buttons = {0x00, 0x09, 0x55},
        .controls = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x34, 0xab},
    };

    wheel_packet_packed_normalize(&input);

    assert(input.buttons[0] == 0x00);
    assert(input.buttons[1] == 0x09);
    assert(input.buttons[2] == 0x55);
    assert(input.controls[0] == 0x0b);
    assert(input.controls[1] == 0x0a);
    assert(input.controls[2] == 0x33);
    assert(input.controls[6] == 0x34);
    assert(input.controls[7] == 0xab);

    input.buttons[1] = 0x01;
    wheel_packet_packed_normalize(&input);
    assert(input.buttons[1] == 0x08);
}

static void test_builds_complete_snapshot(void) {
    uint8_t request[WHEEL_PACKET_PACKED_REQUEST_SIZE] = {0};
    for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_SNAPSHOT_SIZE; index++) {
        request[index + 2] = (uint8_t)(index + 1);
    }
    WheelPacketPackedInput input;
    uint8_t snapshot[WHEEL_PACKET_PACKED_SNAPSHOT_SIZE] = {0};

    wheel_packet_packed_decode(request, &input);
    wheel_packet_packed_snapshot(&input, snapshot);

    assert(memcmp(snapshot, &request[2], sizeof(snapshot)) == 0);
}

static void test_encodes_the_complete_response(void) {
    const WheelDisplayOutput display = {
        .glyphs = {0x11, 0x22, 0x33},
        .third_glyph_marker = true,
    };
    const uint8_t vibration[2] = {0x44, 0x55};
    const uint8_t legacy_axes[2] = {0x66, 0x77};
    uint8_t response[WHEEL_PACKET_PACKED_RESPONSE_SIZE] = {0};
    const uint8_t expected[WHEEL_PACKET_PACKED_RESPONSE_SIZE] = {
        0xa6, 0x00, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };

    wheel_packet_packed_encode(&display, vibration, legacy_axes, response);

    assert(memcmp(response, expected, sizeof(expected)) == 0);
}

int main(void) {
    test_identifies_packed_modes();
    test_decodes_packed_input_fields();
    test_filters_buttons_across_three_samples();
    test_normalizes_buttons_and_packed_axes();
    test_builds_complete_snapshot();
    test_encodes_the_complete_response();
    return 0;
}
