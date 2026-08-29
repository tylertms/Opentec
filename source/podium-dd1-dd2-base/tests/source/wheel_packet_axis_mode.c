#include <assert.h>
#include <stdint.h>

#include "wheel/packet_axis_mode.h"

static void test_selects_axis_mode_packet_family(void) {
    for (uint16_t mode = 0; mode <= UINT8_MAX; mode++) {
        bool expected = mode == 0x09 || mode == 0x0b || mode == 0x1d;
        assert(wheel_packet_axis_mode_applies((uint8_t)mode) == expected);
    }
}

static void test_filters_buttons_and_axes_across_three_samples(void) {
    WheelPacketAxisModeFilter filter;
    WheelPacketAxisModeInput input = {0};
    wheel_packet_axis_mode_filter_init(&filter);

    input.buttons[0] = 0xf3;
    input.buttons[1] = 0x5f;
    input.buttons[2] = 0xff;
    input.controls[4] = 30;
    input.controls[5] = 60;
    wheel_packet_axis_mode_filter(&filter, &input);
    assert(input.buttons[0] == 0);
    assert(input.buttons[1] == 0);
    assert(input.buttons[2] == 0);
    assert(input.controls[4] == 10);
    assert(input.controls[5] == 20);

    input.buttons[0] = 0x73;
    input.buttons[1] = 0x7f;
    input.buttons[2] = 0xf0;
    input.controls[4] = 60;
    input.controls[5] = 90;
    wheel_packet_axis_mode_filter(&filter, &input);
    assert(input.buttons[0] == 0);
    assert(input.controls[4] == 30);
    assert(input.controls[5] == 50);

    input.buttons[0] = 0x7b;
    input.buttons[1] = 0x5e;
    input.buttons[2] = 0xf8;
    input.controls[4] = 90;
    input.controls[5] = 120;
    wheel_packet_axis_mode_filter(&filter, &input);
    assert(input.buttons[0] == 0x73);
    assert(input.buttons[1] == 0x5e);
    assert(input.buttons[2] == 0xf0);
    assert(input.controls[4] == 60);
    assert(input.controls[5] == 90);
    assert(filter.next_sample == 0);
}

static void test_expands_packed_output_nibbles(void) {
    WheelPacketAxisModeInput input = {0};
    input.controls[0] = UINT8_MAX;
    input.controls[1] = UINT8_MAX;
    input.controls[4] = 0x24;
    input.controls[5] = 0x68;
    input.controls[6] = 3;
    input.controls[7] = 0xa5;

    wheel_packet_axis_mode_expand_controls(&input);

    assert(input.controls[0] == 5);
    assert(input.controls[1] == 10);
    assert(input.controls[4] == 0x24);
    assert(input.controls[5] == 0x68);
    assert(input.controls[6] == 3);
    assert(input.controls[7] == 0xa5);
}

int main(void) {
    test_selects_axis_mode_packet_family();
    test_filters_buttons_and_axes_across_three_samples();
    test_expands_packed_output_nibbles();
    return 0;
}
