#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_crc.h"

static void test_selects_crc_modes(void) {
    for (uint8_t mode = 0; mode <= 0x1e; mode++) {
        assert(wheel_packet_crc_applies(mode) == (mode == 6 || mode == 0x15));
    }
}

static void test_decodes_crc_fields(void) {
    uint8_t request[WHEEL_PACKET_CRC_REQUEST_SIZE] = {0};
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_SNAPSHOT_SIZE; index++) {
        request[index + 2] = (uint8_t)(index + 1);
    }

    WheelPacketCrcInput input;
    wheel_packet_crc_decode(request, &input);

    assert(input.buttons[0] == 1);
    assert(input.buttons[2] == 3);
    assert(input.axis_outputs[0] == 4);
    assert(input.axis_outputs[1] == 5);
    assert(input.motion == 6);
    assert(input.controls[0] == 7);
    assert(input.controls[7] == 14);
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

static void test_filters_three_button_and_five_control_bytes(void) {
    WheelPacketCrcFilter filter;
    WheelPacketCrcInput input = {0};
    wheel_packet_crc_filter_init(&filter);

    const uint8_t button_samples[3][3] = {
        {0xf3, 0x5a, 0xff}, {0xf7, 0x7a, 0x7f}, {0xfb, 0x5e, 0xff}};
    const uint8_t control_samples[3][5] = {
        {0xff, 0xf3, 0x5a, 0xff, 0x7f},
        {0x7f, 0xf7, 0x7a, 0x7f, 0xff},
        {0xff, 0xfb, 0x5e, 0xff, 0xff},
    };
    for (uint8_t sample = 0; sample < 3; sample++) {
        memcpy(input.buttons, button_samples[sample], sizeof(input.buttons));
        memcpy(input.controls, control_samples[sample], 5);
        wheel_packet_crc_filter(&filter, &input, 6);
    }

    assert(input.buttons[0] == 0xf3);
    assert(input.buttons[1] == 0x5a);
    assert(input.buttons[2] == 0x7f);
    assert(input.controls[0] == 0x7f);
    assert(input.controls[1] == 0xf3);
    assert(input.controls[2] == 0x5a);
    assert(input.controls[3] == 0x7f);
    assert(input.controls[4] == 0x7f);
}

static void test_filters_pulse_controls_with_button_history(void) {
    WheelPacketCrcFilter filter;
    WheelPacketCrcInput input = {0};
    wheel_packet_crc_filter_init(&filter);
    filter.next_control_sample = 2;

    for (uint8_t sample = 0; sample < WHEEL_PACKET_CRC_HISTORY_DEPTH; sample++) {
        input.buttons[0] = 0x80;
        input.controls[0] = 0x40;
        input.controls[1] = 0x20;
        input.controls[2] = 0x10;
        input.controls[3] = 0xff;
        wheel_packet_crc_filter(&filter, &input, 0x18);
    }

    assert(input.buttons[0] == 0x80);
    assert(input.controls[0] == 0x40);
    assert(input.controls[1] == 0x20);
    assert(input.controls[2] == 0x10);
    assert(input.controls[3] == 0xff);
    assert(filter.next_control_sample == 2);
}

static void test_averages_three_axis_control_samples(void) {
    WheelPacketCrcFilter filter;
    WheelPacketCrcInput input = {0};
    wheel_packet_crc_filter_init(&filter);

    input.controls[5] = 30;
    input.controls[6] = 60;
    wheel_packet_crc_smooth_axes(&filter, &input);
    assert(input.controls[5] == 10);
    assert(input.controls[6] == 20);

    input.controls[5] = 30;
    input.controls[6] = 90;
    wheel_packet_crc_smooth_axes(&filter, &input);
    assert(input.controls[5] == 20);
    assert(input.controls[6] == 50);

    input.controls[5] = 30;
    input.controls[6] = 120;
    wheel_packet_crc_smooth_axes(&filter, &input);
    assert(input.controls[5] == 30);
    assert(input.controls[6] == 90);

    input.controls[5] = 15;
    input.controls[6] = 30;
    wheel_packet_crc_smooth_axes(&filter, &input);
    assert(input.controls[5] == 25);
    assert(input.controls[6] == 80);
}

static void test_prepares_authenticated_podium_buttons(void) {
    WheelPacketCrcInput input = {
        .buttons = {0, 0xff, 0},
        .controls = {0x02, 0x20},
    };

    wheel_packet_crc_prepare(&input, 0x15, 0);

    assert(input.buttons[1] == 0xf6);
    assert(input.buttons[2] == 0x0c);
    assert(input.controls[0] == 0);
    assert(input.controls[1] == 0);

    input.buttons[1] = 0xff;
    input.controls[0] = 0x02;
    input.controls[1] = 0x20;
    wheel_packet_crc_prepare(&input, 6, 0);
    assert(input.buttons[1] == 0xff);
    assert(input.controls[0] == 0x02);
    assert(input.controls[1] == 0x20);
}

static void test_maps_standard_buttons_and_builds_snapshot(void) {
    WheelPacketCrcInput input = {
        .axis_outputs = {0x11, 0x22},
        .motion = -2,
        .controls = {0xff, 0xff, 0xff, 0xff, 0x0a, 5, 6, 7},
        .axis_values = {0x1234, 0xabcd},
        .axis_limit = 0x77,
    };
    uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE];

    wheel_packet_crc_normalize(&input, 6, 7, 0);
    wheel_packet_crc_snapshot(&input, 0x5a, snapshot);

    assert(input.buttons[0] == 0xff);
    assert(input.buttons[1] == 0xff);
    assert(input.buttons[2] == 0x04);
    const uint8_t expected_controls[WHEEL_PACKET_CRC_CONTROL_COUNT] = {0x38, 0x80, 0, 0,
                                                                       0x0a, 5,    6, 7};
    assert(memcmp(input.controls, expected_controls, sizeof(expected_controls)) == 0);
    assert(snapshot[0] == 0xff);
    assert(snapshot[1] == 0xff);
    assert(snapshot[2] == 0x04);
    assert(snapshot[3] == 0x11);
    assert(snapshot[4] == 0x22);
    assert(snapshot[5] == 0xfe);
    assert(snapshot[16] == 0x34);
    assert(snapshot[17] == 0x12);
    assert(snapshot[29] == 0x77);
    assert(snapshot[30] == 0x5a);
}

static void test_writes_exact_official_snapshot_length(void) {
    WheelPacketCrcInput input = {0};
    uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE + 1];
    memset(snapshot, 0xa5, sizeof(snapshot));

    wheel_packet_crc_snapshot(&input, 0x6c, snapshot);

    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_SNAPSHOT_SIZE - 1; index++) {
        assert(snapshot[index] == 0);
    }
    assert(snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE - 1] == 0x6c);
    assert(snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE] == 0xa5);
}

static void test_maps_direct_xbox_buttons(void) {
    WheelPacketCrcInput input = {.controls = {0xff, 0xff, 0xff, 0xff, 0xff}};

    wheel_packet_crc_normalize(&input, 6, 6, 0);

    assert(input.buttons[0] == 0xff);
    assert(input.buttons[1] == 0xff);
    assert(input.buttons[2] == 0xc8);

    memset(&input, 0, sizeof(input));
    input.controls[1] = 0x04;
    wheel_packet_crc_normalize(&input, 6, 6, 0);
    assert(input.buttons[0] == 0x10);
    assert(input.buttons[1] == 0x10);
    assert(input.buttons[2] == 0);

    memset(&input, 0, sizeof(input));
    input.controls[3] = 0x20;
    wheel_packet_crc_normalize(&input, 6, 6, 0);
    assert(input.buttons[0] == 0x02);
    assert(input.buttons[1] == 0);
    assert(input.buttons[2] == 0);
}

static void test_does_not_map_authenticated_standard_auxiliary_bit(void) {
    WheelPacketCrcInput input = {.controls = {0, 0x08}};

    wheel_packet_crc_normalize(&input, 0x15, 0, 0);

    assert(input.buttons[2] == 0);
}

static void test_normalizes_pulse_mode_with_temporary_control(void) {
    WheelPacketCrcInput input = {0};

    wheel_packet_crc_normalize(&input, 0x18, 0, 0);

    assert(input.buttons[1] == 0x08);
    assert(input.controls[2] == 0);
}

static void test_merges_adapter_buttons_axes_and_motion(void) {
    WheelPacketCrcInput input = {.buttons = {0xf0, 0xf0, 0}};
    WheelAdapterInput adapter = {
        .buttons = {0x8f, 0x23, 0x1c},
        .axes = {0x10, 0x20},
        .mode = 2,
        .primary_delta = 2,
        .connected = true,
    };

    wheel_packet_crc_normalize(&input, 6, 0, &adapter);

    assert(input.buttons[0] == 0x4f);
    assert(input.buttons[1] == 0xc4);
    assert(input.buttons[2] == 0x64);
    assert(input.axis_outputs[0] == 0x5f);
    assert(input.axis_outputs[1] == 0x6f);
    assert(input.motion == 1);
    assert(adapter.primary_delta == 1);
    assert(adapter.buttons_active);

    memset(&input, 0, sizeof(input));
    adapter.mode = 1;
    adapter.primary_delta = -1;
    wheel_packet_crc_normalize(&input, 6, 0, &adapter);
    assert(input.buttons[0] == 0x0f);
    assert(input.buttons[1] == 0x80);
    assert(input.buttons[2] == 0x20);
    assert(input.motion == -1);
    assert(adapter.primary_delta == 0);
}

static void test_uses_adapter_xbox_button_sources(void) {
    WheelPacketCrcInput input = {.controls = {0, 0, 0x03}};
    WheelAdapterInput adapter = {.connected = true, .mode = 1};

    wheel_packet_crc_normalize(&input, 6, 6, &adapter);

    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0);
    assert(input.buttons[2] == 0xc0);
}

static void test_encodes_official_crc_response_layout(void) {
    WheelPacketCrcOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .third_glyph_marker = true},
        .legacy_axes = {0x44, 0x55},
        .report_state = 0x66,
        .status_update_pending = true,
    };
    uint8_t response[WHEEL_PACKET_CRC_RESPONSE_SIZE] = {
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
        0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85,
        0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
    };

    wheel_packet_crc_encode(6, &output, response);

    const uint8_t expected[] = {
        0xa5, 0x00, 0x11, 0x22, 0xb3, 0x75, 0x76, 0x44, 0x55, 0x66, 0xff,
        0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85,
        0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x89,
    };
    assert(memcmp(response, expected, sizeof(expected)) == 0);
    assert(!output.status_update_pending);

    memset(response, 0xa0, sizeof(response));
    wheel_packet_crc_encode(0x15, &output, response);

    const uint8_t expected_authenticated[] = {
        0xa6, 0x00, 0x11, 0x22, 0xb3, 0xa0, 0xa0, 0x44, 0x55, 0x66, 0x00,
        0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
        0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe9,
    };
    assert(memcmp(response, expected_authenticated, sizeof(expected_authenticated)) == 0);
}

int main(void) {
    test_selects_crc_modes();
    test_decodes_crc_fields();
    test_filters_three_button_and_five_control_bytes();
    test_filters_pulse_controls_with_button_history();
    test_averages_three_axis_control_samples();
    test_prepares_authenticated_podium_buttons();
    test_maps_standard_buttons_and_builds_snapshot();
    test_writes_exact_official_snapshot_length();
    test_maps_direct_xbox_buttons();
    test_does_not_map_authenticated_standard_auxiliary_bit();
    test_normalizes_pulse_mode_with_temporary_control();
    test_merges_adapter_buttons_axes_and_motion();
    test_uses_adapter_xbox_button_sources();
    test_encodes_official_crc_response_layout();
    return 0;
}
