#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_mode_four.h"

static void test_decodes_mode_four_fields(void) {
    uint8_t request[WHEEL_PACKET_MODE_FOUR_REQUEST_SIZE] = {0};
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE; index++) {
        request[index + 2] = (uint8_t)(index + 1);
    }

    WheelPacketModeFourInput input;
    wheel_packet_mode_four_decode(request, &input);

    assert(input.buttons[0] == 1);
    assert(input.buttons[2] == 3);
    assert(input.axis_outputs[0] == 4);
    assert(input.axis_outputs[1] == 5);
    assert(input.motion == 6);
    assert(input.controls[0] == 7);
    assert(input.controls[3] == 10);
    assert(input.control_data[0] == 11);
    assert(input.control_data[3] == 14);
    assert(input.axis_values[0] == 0x1211);
    assert(input.axis_values[1] == 0x1413);
    assert(input.mode_buttons == 21);
    assert(input.axis_report_enabled == 22);
    assert(input.auxiliary_data[0] == 23);
    assert(input.auxiliary_data[3] == 26);
    assert(input.report_mode == 27);
    assert(input.report_capabilities == 29);
    assert(input.axis_limit == 30);
}

static void test_filters_buttons_across_three_samples(void) {
    WheelPacketModeFourFilter filter;
    WheelPacketModeFourInput input = {0};
    wheel_packet_mode_four_filter_init(&filter);

    const uint8_t samples[3][3] = {{0xf3, 0x5a, 0xff}, {0xf7, 0x7a, 0x7f}, {0xfb, 0x5e, 0xff}};
    for (uint8_t sample = 0; sample < 3; sample++) {
        memcpy(input.buttons, samples[sample], sizeof(input.buttons));
        wheel_packet_mode_four_filter(&filter, &input);
    }

    assert(input.buttons[0] == 0xf3);
    assert(input.buttons[1] == 0x5a);
    assert(input.buttons[2] == 0x7f);
}

static void test_filters_controls_across_four_samples(void) {
    WheelPacketModeFourFilter filter;
    WheelPacketModeFourInput input = {0};
    wheel_packet_mode_four_filter_init(&filter);

    const uint8_t samples[4][4] = {{0xff, 0xf3, 0x5a, 0xff},
                                   {0x7f, 0xf7, 0x7a, 0x7f},
                                   {0xff, 0xfb, 0x5e, 0xff},
                                   {0xff, 0xf3, 0x5a, 0xff}};
    for (uint8_t sample = 0; sample < 4; sample++) {
        memcpy(input.controls, samples[sample], sizeof(input.controls));
        wheel_packet_mode_four_filter(&filter, &input);
    }

    assert(input.controls[0] == 0xff);
    assert(input.controls[1] == 0xf3);
    assert(input.controls[2] == 0x5a);
    assert(input.controls[3] == 0xff);
}

static void test_maps_legacy_controls_and_builds_snapshot(void) {
    WheelPacketModeFourInput input = {
        .buttons = {0, 0, 0x33},
        .axis_outputs = {0x44, 0x55},
        .motion = -2,
        .controls = {0xff, 0xff, 0x01, 0x01},
        .control_data = {0x61, 0x62, 0x63, 0x64},
        .reserved_axes = {0x65, 0x66},
        .axis_values = {0x1234, 0xabcd},
        .mode_buttons = 0x05,
        .axis_report_enabled = 1,
        .auxiliary_data = {0x71, 0x72, 0x73, 0x74},
        .report_mode = 0x75,
        .reserved_report = 0x76,
        .report_capabilities = 0x77,
        .axis_limit = 0x78,
    };
    WheelPacketModeFourRuntime runtime = {0};
    uint8_t snapshot[WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE];

    wheel_packet_mode_four_normalize(&input, 4, &runtime, snapshot);

    assert(input.buttons[0] == 0x20);
    assert(input.buttons[1] == 0x08);
    assert(input.controls[0] == 0x38);
    assert(input.controls[1] == 0x80);
    assert(input.controls[2] == 0);
    assert(input.controls[3] == 0);
    assert(runtime.extended_buttons == 0);
    assert(runtime.axis_report_enabled == 1);
    assert(snapshot[0] == 0x20);
    assert(snapshot[1] == 0x08);
    assert(snapshot[5] == 0xfe);
    assert(snapshot[6] == 0x38);
    assert(snapshot[7] == 0x80);
    assert(snapshot[10] == 0x61);
    assert(snapshot[16] == 0x34);
    assert(snapshot[17] == 0x12);
    assert(snapshot[20] == 0x05);
    assert(snapshot[29] == 0x78);
}

static void test_maps_playstation_mode_buttons_and_latches_runtime(void) {
    WheelPacketModeFourInput input = {.buttons = {0, 0xff, 0}, .mode_buttons = 0x0f};
    WheelPacketModeFourRuntime runtime = {0};
    uint8_t snapshot[WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE];

    wheel_packet_mode_four_normalize(&input, 7, &runtime, snapshot);

    assert(input.buttons[1] == 0xff);
    assert(runtime.extended_buttons == 0x05);
    assert(runtime.axis_report_enabled == 1);

    input.mode_buttons = 0;
    input.axis_report_enabled = 0;
    wheel_packet_mode_four_normalize(&input, 7, &runtime, snapshot);
    assert(runtime.extended_buttons == 0x05);
    assert(runtime.axis_report_enabled == 1);
}

static void test_encodes_mode_four_response(void) {
    const WheelPacketModeFourOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .third_glyph_marker = true},
        .vibration = {0x44, 0x55},
        .legacy_axes = {0x66, 0x77},
    };
    uint8_t response[WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE] = {0};
    const uint8_t expected[WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE] = {
        0xa5, 0x00, 0x11, 0x22, 0xb3, 0x44, 0x55, 0x66, 0x77,
    };

    wheel_packet_mode_four_encode(&output, response);

    assert(memcmp(response, expected, sizeof(expected)) == 0);
}

int main(void) {
    test_decodes_mode_four_fields();
    test_filters_buttons_across_three_samples();
    test_filters_controls_across_four_samples();
    test_maps_legacy_controls_and_builds_snapshot();
    test_maps_playstation_mode_buttons_and_latches_runtime();
    test_encodes_mode_four_response();
    return 0;
}
