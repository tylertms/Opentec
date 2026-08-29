#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "wheel/packet_common.h"
#include "wheel/packet_remapped.h"

static void test_identifies_remapped_mode(void) {
    assert(wheel_packet_remapped_applies(0x11));
    assert(!wheel_packet_remapped_applies(0x10));
    assert(!wheel_packet_remapped_applies(0x12));
}

static void test_filters_unmodified_buttons(void) {
    WheelPacketRemappedFilter filter;
    wheel_packet_remapped_filter_init(&filter);
    WheelPacketRemappedInput input = {.buttons = {0xf3, 0x5a, 0xff}};
    wheel_packet_remapped_filter(&filter, &input, 0);
    assert(memcmp(input.buttons, (uint8_t[3]){0}, 3) == 0);
    input = (WheelPacketRemappedInput){.buttons = {0xf7, 0x7a, 0x7f}};
    wheel_packet_remapped_filter(&filter, &input, 0);
    assert(memcmp(input.buttons, (uint8_t[3]){0}, 3) == 0);
    input = (WheelPacketRemappedInput){.buttons = {0xfb, 0x5e, 0xff}};
    wheel_packet_remapped_filter(&filter, &input, 0);
    assert(memcmp(input.buttons, (uint8_t[]){0xf3, 0x5a, 0x7f}, 3) == 0);
}

static void test_remaps_playstation_buttons_before_filtering(void) {
    WheelPacketRemappedFilter filter;
    wheel_packet_remapped_filter_init(&filter);
    for (uint8_t sample = 0; sample < WHEEL_PACKET_REMAPPED_HISTORY_DEPTH; sample++) {
        WheelPacketRemappedInput input = {.buttons = {0, 0x01, 0x03}};
        wheel_packet_remapped_filter(&filter, &input, 7);
        if (sample == WHEEL_PACKET_REMAPPED_HISTORY_DEPTH - 1) {
            assert(input.buttons[0] == 0x10);
            assert(input.buttons[1] == 0x08);
            assert(input.buttons[2] == 0x03);
        }
    }

    wheel_packet_remapped_filter_init(&filter);
    for (uint8_t sample = 0; sample < WHEEL_PACKET_REMAPPED_HISTORY_DEPTH; sample++) {
        WheelPacketRemappedInput input = {.buttons = {0x10, 0x08, 0}};
        wheel_packet_remapped_filter(&filter, &input, 7);
        if (sample == WHEEL_PACKET_REMAPPED_HISTORY_DEPTH - 1) {
            assert(input.buttons[0] == 0);
            assert(input.buttons[1] == 0x01);
            assert(input.buttons[2] == 0);
        }
    }
}

static void test_decodes_primary_motion_flags(void) {
    WheelPacketRemappedInput input = {.motion = 0x10};
    assert(wheel_packet_remapped_primary_delta(&input) == 1);
    input.motion = 0x20;
    assert(wheel_packet_remapped_primary_delta(&input) == -1);
    input.motion = 0x30;
    assert(wheel_packet_remapped_primary_delta(&input) == 1);
    input.motion = 0x40;
    assert(wheel_packet_remapped_primary_delta(&input) == 0);
}

static void test_uses_the_common_codec(void) {
    uint8_t request[WHEEL_PACKET_COMMON_REQUEST_SIZE] = {0};
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_SNAPSHOT_SIZE; index++) {
        request[index + 2] = (uint8_t)(index + 1);
    }
    WheelPacketRemappedInput input;
    uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE];
    wheel_packet_common_decode(request, &input);
    wheel_packet_common_snapshot(&input, snapshot);
    assert(memcmp(snapshot, request + 2, sizeof(snapshot)) == 0);
}

int main(void) {
    test_identifies_remapped_mode();
    test_filters_unmodified_buttons();
    test_remaps_playstation_buttons_before_filtering();
    test_decodes_primary_motion_flags();
    test_uses_the_common_codec();
    return 0;
}
