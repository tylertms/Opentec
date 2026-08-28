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

static void test_applies_v3_report_set(void) {
    PedalV3State state;
    PedalInput input = {
        .auxiliary = 0x55,
    };
    pedal_v3_state_init(&state);

    const PedalFrame sample = {
        .type = 1,
        .payload = {0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0, 0xde},
    };
    assert(pedal_v3_apply_report(&sample, true, &state, &input));
    assert(input.axes[0] == 0x1234);
    assert(input.axes[1] == 0x5678);
    assert(input.axes[2] == 0x9abc);
    assert(input.auxiliary == 0x55);
    assert(state.raw_brake == 0x5678);

    const PedalFrame connection = {
        .type = 4,
        .payload = {0xa5},
    };
    assert(pedal_v3_apply_report(&connection, false, &state, &input));
    assert(state.connection_flags == 0xa5);

    const PedalFrame primary_calibration = {
        .type = 5,
        .payload = {0x05, 0x62},
    };
    assert(pedal_v3_apply_report(&primary_calibration, false, &state, &input));
    assert(state.primary_calibration);
    assert(!state.legacy_calibration);

    const PedalFrame fine_brake_force = {
        .type = 7,
        .payload = {3},
    };
    assert(pedal_v3_apply_report(&fine_brake_force, false, &state, &input));
    assert(state.alternate_brake_force == 10);

    const PedalFrame legacy_calibration = {
        .type = 5,
        .payload = {0x3b, 0x18},
    };
    assert(pedal_v3_apply_report(&legacy_calibration, false, &state, &input));
    assert(state.primary_calibration);
    assert(state.legacy_calibration);

    const PedalFrame normal_brake_force = {
        .type = 7,
        .payload = {3},
    };
    assert(pedal_v3_apply_report(&normal_brake_force, false, &state, &input));
    assert(state.alternate_brake_force == 20);

    const PedalFrame secondary_calibration = {
        .type = 5,
        .payload = {0x06, 0x62},
    };
    assert(pedal_v3_apply_report(&secondary_calibration, false, &state, &input));
    assert(!state.legacy_calibration);
    assert(state.secondary_calibration);

    const PedalFrame shared_axes = {
        .type = 8,
        .payload = {7, 8, 9},
    };
    assert(pedal_v3_apply_report(&shared_axes, false, &state, &input));
    assert(state.shared_axes[0] == 7);
    assert(state.shared_axes[1] == 8);
    assert(state.shared_axes[2] == 9);

    const PedalFrame unknown = {
        .type = 9,
    };
    assert(!pedal_v3_apply_report(&unknown, false, &state, &input));
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
    test_applies_v3_report_set();
    test_scales_brake_force();
    test_released_hid_axes_are_high();
    return 0;
}
