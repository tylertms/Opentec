#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_mode_one.h"

static void test_identifies_shared_codec_modes(void) {
    assert(wheel_packet_mode_one_applies(1));
    assert(wheel_packet_mode_one_applies(2));
    assert(wheel_packet_mode_one_applies(3));
    assert(wheel_packet_mode_one_applies(0x13));
    assert(wheel_packet_mode_one_applies(0x14));
    assert(wheel_packet_mode_one_applies(0x16));
    assert(!wheel_packet_mode_one_applies(0));
    assert(!wheel_packet_mode_one_applies(0x12));
    assert(!wheel_packet_mode_one_applies(0x15));
}

static void test_encodes_the_complete_response(void) {
    const WheelPacketModeOneOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .third_glyph_marker = true},
        .vibration = {0x44, 0x55},
        .legacy_axes = {0x66, 0x77},
    };
    uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {0};
    const uint8_t expected[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {
        0xa5, 0x00, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };

    wheel_packet_mode_one_encode(0x12, &output, response);
    assert(memcmp(response, expected, sizeof(expected)) == 0);
}

static void test_requests_authentication_for_authenticated_packet_modes(void) {
    const WheelPacketModeOneOutput output = {0};
    uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE] = {0};

    wheel_packet_mode_one_encode(0x13, &output, response);
    assert(response[0] == 0xa6);

    wheel_packet_mode_one_encode(0x14, &output, response);
    assert(response[0] == 0xa6);

    wheel_packet_mode_one_encode(0x16, &output, response);
    assert(response[0] == 0xa6);

    wheel_packet_mode_one_encode(2, &output, response);
    assert(response[0] == 0xa5);

    wheel_packet_mode_one_encode(0x15, &output, response);
    assert(response[0] == 0xa5);
}

static void test_decodes_standard_input_fields(void) {
    uint8_t request[WHEEL_PACKET_MODE_ONE_REQUEST_SIZE] = {0};
    const uint8_t payload[] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0xfe, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0xa0,
        0xa1, 0x34, 0x12, 0xcd, 0xab, 0x70, 0x71, 0xa2, 0xa3, 0xa4, 0xa5, 0x72, 0xa6, 0x73, 0x74,
    };
    memcpy(&request[2], payload, sizeof(payload));

    WheelPacketModeOneInput input;
    wheel_packet_mode_one_decode(request, &input);

    assert(input.buttons[0] == 0x11);
    assert(input.buttons[1] == 0x22);
    assert(input.buttons[2] == 0x33);
    assert(input.axis_outputs[0] == 0x44);
    assert(input.axis_outputs[1] == 0x55);
    assert(input.motion == -2);
    assert(input.controls.values[0] == 0x61);
    assert(input.controls.values[1] == 0x62);
    assert(input.controls.enabled == 0x63);
    assert(input.controls.latch_flags == 0x64);
    assert(input.controls.x == 0x65);
    assert(input.controls.y == 0x66);
    assert(input.controls.mode == 0x67);
    assert(input.controls.packed_values == 0x68);
    assert(input.axis_values[0] == 0x1234);
    assert(input.axis_values[1] == 0xabcd);
    assert(input.mode_buttons == 0x70);
    assert(input.axis_report_enabled == 0x71);
    assert(input.report_mode == 0x72);
    assert(input.report_capabilities == 0x73);
    assert(input.axis_limit == 0x74);
}

static void test_filters_buttons_across_three_samples(void) {
    WheelPacketModeOneButtonFilter filter;
    wheel_packet_mode_one_button_filter_init(&filter);

    WheelPacketModeOneInput input = {.buttons = {0xf3, 0x5a, 0xff}};
    wheel_packet_mode_one_filter_buttons(&filter, &input);
    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0);
    assert(input.buttons[2] == 0);

    input = (WheelPacketModeOneInput){.buttons = {0xf7, 0x7a, 0x7f}};
    wheel_packet_mode_one_filter_buttons(&filter, &input);
    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0);
    assert(input.buttons[2] == 0);

    input = (WheelPacketModeOneInput){.buttons = {0xfb, 0x5e, 0xff}};
    wheel_packet_mode_one_filter_buttons(&filter, &input);
    assert(input.buttons[0] == 0xf3);
    assert(input.buttons[1] == 0x5a);
    assert(input.buttons[2] == 0x7f);
}

static void test_averages_authenticated_control_axes_across_three_samples(void) {
    WheelPacketModeOneControlAxisFilter filter;
    wheel_packet_mode_one_control_axis_filter_init(&filter);

    WheelPacketModeOneInput input = {.controls = {.x = 90, .y = 30}};
    wheel_packet_mode_one_filter_control_axes(&filter, &input);
    assert(input.controls.x == 30);
    assert(input.controls.y == 10);

    input = (WheelPacketModeOneInput){.controls = {.x = 60, .y = 90}};
    wheel_packet_mode_one_filter_control_axes(&filter, &input);
    assert(input.controls.x == 50);
    assert(input.controls.y == 40);

    input = (WheelPacketModeOneInput){.controls = {.x = 30, .y = 60}};
    wheel_packet_mode_one_filter_control_axes(&filter, &input);
    assert(input.controls.x == 60);
    assert(input.controls.y == 60);

    input = (WheelPacketModeOneInput){.controls = {.x = 0, .y = 0}};
    wheel_packet_mode_one_filter_control_axes(&filter, &input);
    assert(input.controls.x == 30);
    assert(input.controls.y == 50);
}

static void test_builds_normalized_snapshots(void) {
    WheelPacketModeOneInput input = {
        .buttons = {0x11, 0x08, 0x33},
        .axis_outputs = {0x44, 0x55},
        .motion = -2,
        .controls = {.values = {0x61, 0x62},
                     .enabled = 1,
                     .latch_flags = 1,
                     .x = 0x65,
                     .y = 0x66,
                     .mode = 4,
                     .packed_values = 0x68},
        .axis_values = {0x1234, 0xabcd},
        .mode_buttons = 0x70,
        .axis_report_enabled = 0x71,
        .report_mode = 0x72,
        .report_capabilities = 0x73,
        .axis_limit = 0x74,
    };
    uint8_t snapshot[WHEEL_PACKET_MODE_ONE_SNAPSHOT_SIZE];
    wheel_packet_mode_one_normalize(&input, true, true, false, snapshot);

    assert(input.buttons[1] == 0x09);
    assert(input.controls.latch_flags == 3);
    assert(input.controls.values[0] == 0);
    assert(input.controls.x == 0);
    assert(input.axis_values[0] == 0);
    assert(snapshot[0] == 0x11);
    assert(snapshot[1] == 0x09);
    assert(snapshot[2] == 0x33);
    assert(snapshot[3] == 0x44);
    assert(snapshot[4] == 0x55);
    assert(snapshot[5] == 0xfe);
    assert(snapshot[9] == 3);
    assert(snapshot[29] == 0x74);
    for (uint8_t index = 6; index < 29; index++) {
        if (index != 9) {
            assert(snapshot[index] == 0);
        }
    }

    input.controls.latch_flags = 3;
    wheel_packet_mode_one_normalize(&input, false, true, false, snapshot);
    assert(input.controls.latch_flags == 0);
    assert(snapshot[9] == 0);
}

int main(void) {
    test_identifies_shared_codec_modes();
    test_encodes_the_complete_response();
    test_requests_authentication_for_authenticated_packet_modes();
    test_decodes_standard_input_fields();
    test_filters_buttons_across_three_samples();
    test_averages_authenticated_control_axes_across_three_samples();
    test_builds_normalized_snapshots();
    return 0;
}
