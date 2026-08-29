#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_display.h"

static void test_identifies_standard_display_mode(void) {
    assert(wheel_packet_display_applies(0x10));
    assert(!wheel_packet_display_applies(0x0f));
    assert(!wheel_packet_display_applies(0x11));
}

static void test_decodes_all_logical_fields(void) {
    uint8_t request[WHEEL_PACKET_DISPLAY_REQUEST_SIZE] = {0};
    const uint8_t payload[WHEEL_PACKET_DISPLAY_SNAPSHOT_SIZE] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0xfe, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x34, 0x12, 0xcd, 0xab, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    };
    memcpy(request + 2, payload, sizeof(payload));

    WheelPacketDisplayInput input;
    wheel_packet_common_decode(request, &input);

    assert(memcmp(input.buttons, (uint8_t[]){0x11, 0x22, 0x33}, 3) == 0);
    assert(memcmp(input.axis_outputs, (uint8_t[]){0x44, 0x55}, 2) == 0);
    assert(input.motion == -2);
    assert(memcmp(input.controls, &payload[6], 8) == 0);
    assert(memcmp(input.reserved_axes, (uint8_t[]){0x69, 0x6a}, 2) == 0);
    assert(input.axis_values[0] == 0x1234);
    assert(input.axis_values[1] == 0xabcd);
    assert(input.mode_buttons == 0x70);
    assert(input.axis_report_enabled == 0x71);
    assert(memcmp(input.auxiliary_data, &payload[22], 4) == 0);
    assert(input.report_mode == 0x76);
    assert(input.reserved_report == 0x77);
    assert(input.report_capabilities == 0x78);
    assert(input.axis_limit == 0x79);
}

static void test_filters_buttons_and_leading_controls_together(void) {
    WheelPacketDisplayFilter filter;
    wheel_packet_display_filter_init(&filter);

    WheelPacketDisplayInput input = {
        .buttons = {0xf3, 0x5a, 0xff},
        .controls = {0x7f, 0xf3, 0x5a},
    };
    wheel_packet_display_filter(&filter, &input);
    assert(memcmp(input.buttons, (uint8_t[3]){0}, 3) == 0);
    assert(memcmp(input.controls, (uint8_t[3]){0}, 3) == 0);

    input = (WheelPacketDisplayInput){
        .buttons = {0xf7, 0x7a, 0x7f},
        .controls = {0xff, 0xf7, 0x7a},
    };
    wheel_packet_display_filter(&filter, &input);
    assert(memcmp(input.buttons, (uint8_t[3]){0}, 3) == 0);
    assert(memcmp(input.controls, (uint8_t[3]){0}, 3) == 0);

    input = (WheelPacketDisplayInput){
        .buttons = {0xfb, 0x5e, 0xff},
        .controls = {0x7f, 0xfb, 0x5e},
    };
    wheel_packet_display_filter(&filter, &input);
    assert(memcmp(input.buttons, (uint8_t[]){0xf3, 0x5a, 0x7f}, 3) == 0);
    assert(memcmp(input.controls, (uint8_t[]){0x7f, 0xf3, 0x5a}, 3) == 0);
}

static void test_serializes_the_filtered_snapshot(void) {
    uint8_t request[WHEEL_PACKET_DISPLAY_REQUEST_SIZE] = {0};
    for (uint8_t index = 0; index < WHEEL_PACKET_DISPLAY_SNAPSHOT_SIZE; index++) {
        request[index + 2] = (uint8_t)(index + 1);
    }
    WheelPacketDisplayInput input;
    uint8_t snapshot[WHEEL_PACKET_DISPLAY_SNAPSHOT_SIZE];
    wheel_packet_common_decode(request, &input);
    wheel_packet_common_snapshot(&input, snapshot);
    assert(memcmp(snapshot, request + 2, sizeof(snapshot)) == 0);
}

static void test_encodes_authenticated_display_output(void) {
    const WheelDisplayOutput display = {
        .glyphs = {0x11, 0x22, 0x33},
        .third_glyph_marker = true,
    };
    const uint8_t vibration[2] = {0x44, 0x55};
    const uint8_t axes[2] = {0x66, 0x77};
    uint8_t response[WHEEL_PACKET_DISPLAY_RESPONSE_SIZE] = {0};
    const uint8_t expected[WHEEL_PACKET_DISPLAY_RESPONSE_SIZE] = {
        0xa6, 0, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };

    wheel_packet_common_response_encode(&display, vibration, axes, response);
    assert(memcmp(response, expected, sizeof(expected)) == 0);
}

int main(void) {
    test_identifies_standard_display_mode();
    test_decodes_all_logical_fields();
    test_filters_buttons_and_leading_controls_together();
    test_serializes_the_filtered_snapshot();
    test_encodes_authenticated_display_output();
    return 0;
}
