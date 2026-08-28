#include <assert.h>
#include <stdint.h>

#include "pedal/input.h"

static void test_decodes_axis_sample(void) {
    const PedalFrame frame = {
        .type = PEDAL_FRAME_AXIS_SAMPLE,
        .payload = {0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0, 0xde},
    };
    PedalInput input;

    assert(pedal_input_decode(&frame, &input));
    assert(input.axes[0] == 0x1234);
    assert(input.axes[1] == 0x5678);
    assert(input.axes[2] == 0x9abc);
    assert(input.auxiliary == 0xde);
}

static void test_ignores_other_frames(void) {
    const PedalFrame frame = {
        .type = 4,
        .payload = {0},
    };
    PedalInput input = {
        .axes = {1, 2, 3},
        .auxiliary = 4,
    };

    assert(!pedal_input_decode(&frame, &input));
    assert(input.axes[0] == 1);
    assert(input.axes[1] == 2);
    assert(input.axes[2] == 3);
    assert(input.auxiliary == 4);
}

static void test_scales_brake_force(void) {
    assert(pedal_input_scale_brake(1000, 100) == 1000);
    assert(pedal_input_scale_brake(1000, 50) == 3000);
    assert(pedal_input_scale_brake(1000, 0) == 5000);
    assert(pedal_input_scale_brake(30000, 50) == UINT16_MAX);
    assert(pedal_input_scale_brake(225, 99) == 233);
    assert(pedal_input_scale_brake(1000, 101) == 1000);
}

static void test_released_hid_axes_are_high(void) {
    PedalInput input = {
        .axes = {1, 2, 3},
        .auxiliary = 4,
    };
    pedal_input_release(&input);

    assert(input.axes[0] == 0);
    assert(input.axes[1] == 0);
    assert(input.axes[2] == 0);
    assert(input.auxiliary == 0);
    assert(pedal_input_hid_axis(input.axes[0]) == UINT16_MAX);
    assert(pedal_input_hid_axis(UINT16_MAX) == 0);
    assert(pedal_input_hid_auxiliary(input.auxiliary) == UINT8_MAX);
    assert(pedal_input_hid_auxiliary(UINT8_MAX) == 0);
}

int main(void) {
    test_decodes_axis_sample();
    test_ignores_other_frames();
    test_scales_brake_force();
    test_released_hid_axes_are_high();
    return 0;
}
