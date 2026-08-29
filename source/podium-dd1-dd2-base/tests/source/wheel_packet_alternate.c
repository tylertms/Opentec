#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_alternate.h"

static void filter_three_samples(WheelPacketAlternateFilter *filter,
                                 WheelPacketAlternateInput *input, uint8_t interface_mode) {
    WheelPacketAlternateInput source = *input;
    for (uint8_t sample = 0; sample < WHEEL_PACKET_ALTERNATE_HISTORY_DEPTH; sample++) {
        *input = source;
        wheel_packet_alternate_filter(filter, input, interface_mode);
    }
}

static void test_selects_mode_twelve(void) {
    assert(wheel_packet_alternate_applies(0x12));
    assert(!wheel_packet_alternate_applies(0x11));
    assert(!wheel_packet_alternate_applies(0x13));
}

static void test_remaps_non_console_input(void) {
    WheelPacketAlternateFilter filter;
    WheelPacketAlternateInput input = {
        .buttons = {0x10, 0x01, 0x03},
        .controls = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
    };
    wheel_packet_alternate_filter_init(&filter);
    filter_three_samples(&filter, &input, 0);
    assert(input.buttons[0] == 0x10);
    assert(input.buttons[1] == 0x08);
    assert(input.buttons[2] == 0x0a);
    assert(input.controls[0] == 0x10);
    assert(input.controls[1] == 0x11);
    assert(input.controls[2] == 0);
    assert(input.controls[3] == 0);
    assert(input.controls[4] == 0);
    assert(input.controls[5] == 0);
    assert(input.controls[6] == 0x16);
    assert(input.controls[7] == 0x17);
}

static void test_remaps_xbox_input(void) {
    WheelPacketAlternateFilter filter;
    WheelPacketAlternateInput input = {.buttons = {0, 0x01, 0x03}};
    wheel_packet_alternate_filter_init(&filter);
    filter_three_samples(&filter, &input, 6);
    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0x08);
    assert(input.buttons[2] == 0x07);
}

static void test_remaps_playstation_input(void) {
    WheelPacketAlternateFilter filter;
    WheelPacketAlternateInput input = {.buttons = {0, 0x08, 0x03}};
    wheel_packet_alternate_filter_init(&filter);
    filter_three_samples(&filter, &input, 7);
    assert(input.buttons[0] == 0x10);
    assert(input.buttons[1] == 0x01);
    assert(input.buttons[2] == 0x0b);
}

static void test_encodes_default_output(void) {
    WheelPacketAlternateOutput output = {
        .display = {.glyphs = {0x11, 0x22, 0x33}, .auxiliary = 0x44, .third_glyph_marker = true},
        .report_state = 0x55,
        .auxiliary_link_option = 1,
        .auxiliary_status = true,
        .command_restart_pending = true,
    };
    uint8_t response[WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE];
    memset(response, 0xa5, sizeof(response));
    wheel_packet_alternate_encode(&output, response);
    assert(response[0] == 0xa6);
    assert(response[1] == 2);
    assert(response[2] == 0x11);
    assert(response[3] == 0x22);
    assert(response[4] == 0x33);
    assert(response[5] == 0x44);
    assert(response[6] == 3);
    assert(response[9] == 0x55);
    assert(response[10] == UINT8_MAX);
    assert(response[32] == 0);
    assert(!output.command_restart_pending);

    wheel_packet_alternate_encode(&output, response);
    assert(response[10] == 0);
}

static void test_suppresses_auxiliary_display(void) {
    WheelPacketAlternateOutput output = {
        .display = {.auxiliary = 0x44, .third_glyph_marker = true},
        .auxiliary_status = true,
        .suppress_auxiliary_display = true,
    };
    uint8_t response[WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE];
    wheel_packet_alternate_encode(&output, response);
    assert(response[5] == 0);
    assert(response[6] == 0);
}

static void test_schedules_queued_payload(void) {
    WheelPacketAlternateOutput output = {0};
    uint8_t payload[WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE];
    for (uint8_t index = 0; index < sizeof(payload); index++) {
        payload[index] = (uint8_t)(0x80u + index);
    }
    assert(wheel_packet_alternate_queue_payload(&output, payload));
    assert(!wheel_packet_alternate_queue_payload(&output, payload));
    assert(wheel_packet_alternate_payload_pending(&output));

    uint8_t transfer_count = 0;
    uint8_t response[WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE];
    for (uint8_t exchange = 0; exchange < 31; exchange++) {
        memset(response, 0, sizeof(response));
        wheel_packet_alternate_encode(&output, response);
        if (response[1] == 0x12) {
            transfer_count++;
            assert(memcmp(response + 2, payload, sizeof(payload)) == 0);
        }
    }
    assert(transfer_count == 8);
    assert(output.sequence == 0);
    assert(!wheel_packet_alternate_payload_pending(&output));
}

static void test_suppressed_payload_does_not_advance(void) {
    WheelPacketAlternateOutput output = {.payload_suppressed = true};
    uint8_t payload[WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE] = {1};
    uint8_t response[WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE];
    assert(wheel_packet_alternate_queue_payload(&output, payload));
    wheel_packet_alternate_encode(&output, response);
    assert(output.sequence == 0);
    assert(response[1] == 0);
    assert(wheel_packet_alternate_payload_pending(&output));
}

int main(void) {
    test_selects_mode_twelve();
    test_remaps_non_console_input();
    test_remaps_xbox_input();
    test_remaps_playstation_input();
    test_encodes_default_output();
    test_suppresses_auxiliary_display();
    test_schedules_queued_payload();
    test_suppressed_payload_does_not_advance();
    return 0;
}
