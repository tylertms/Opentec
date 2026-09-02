#include "usb/xbox_gip_input.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static void test_maps_primary_buttons_and_alternates_packets(void) {
    UsbXboxGipInputBuilder builder;
    UsbXboxGipInputSnapshot snapshot;
    UsbXboxGipInputState state = {
        .buttons = {0x11, 0xfe, 0},
        .wheel_mode = 7,
    };

    usb_xbox_gip_input_builder_init(&builder);
    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert(snapshot.buttons[0] == 0x1e);
    assert(snapshot.buttons[1] == 0x11);
    assert(snapshot.button_flags == 0x0f);

    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert(snapshot.buttons[0] == 0x1c);
}

static void test_maps_mode_specific_extensions(void) {
    UsbXboxGipInputBuilder builder;
    UsbXboxGipInputSnapshot snapshot;
    UsbXboxGipInputState state = {
        .buttons = {0, 0, 0xd2},
        .mode_buttons = 0xc0,
        .wheel_mode = 6,
        .controls = {0, 0, 0, 0, 0, 0, 0, 5},
    };

    usb_xbox_gip_input_builder_init(&builder);
    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert(snapshot.button_flags == 0x34);
    assert(snapshot.packed_buttons == 0x30);

    state.wheel_mode = 9;
    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert(snapshot.packed_buttons == 0x0f);

    state.wheel_mode = 10;
    state.mode_buttons = 0xd8;
    state.controls[6] = 3;
    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert(snapshot.packed_buttons == 0x3f);

    state.wheel_mode = 29;
    state.mode_buttons = 4;
    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert((snapshot.buttons[0] & 0x10) == 0);
    assert(snapshot.extended_button == 1);

    state.wheel_mode = 18;
    state.mode_buttons = 2;
    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert((snapshot.buttons[0] & 0x10) != 0);
}

static void test_maps_axes_profile_and_selectors(void) {
    UsbXboxGipInputBuilder builder;
    UsbXboxGipInputSnapshot snapshot;
    UsbXboxGipInputState state = {
        .rotary = {1, 2, 3, 4, 5},
        .steering = 0x1234,
        .pedals = {0x2345, 0x3456, 0x4567},
        .auxiliary_pedal = 0x78,
        .clutch_paddles = {0x89, 0x9a},
        .encoder_direction = -1,
        .axis_mode = 1,
        .led_state = 4,
        .steering_range_units = 108,
        .force_feedback_level = 0x80,
        .pedal_active = {true, false, true},
        .auxiliary_pedal_active = true,
    };

    usb_xbox_gip_input_builder_init(&builder);
    usb_xbox_gip_input_build(&builder, &state, &snapshot);
    assert(snapshot.steering == 0x1234);
    assert(snapshot.pedals[0] == 0x2345 && snapshot.pedals[1] == 0x3456 &&
           snapshot.pedals[2] == 0x4567);
    assert(snapshot.auxiliary_pedal == 0x78);
    assert(snapshot.axis_mode == 1 && snapshot.led_state == 2);
    assert(snapshot.steering_range_units == 108);
    assert(snapshot.force_feedback_level == 0x80);
    assert(snapshot.pedal_active[0] && !snapshot.pedal_active[1] && snapshot.pedal_active[2]);
    assert(snapshot.auxiliary_pedal_active);
    assert(snapshot.clutch_paddles[0] == 0x89 && snapshot.clutch_paddles[1] == 0x9a);
    assert(snapshot.selectors[0] == 1 && snapshot.selectors[1] == 2 && snapshot.selectors[2] == 3 &&
           snapshot.selectors[3] == 1 && snapshot.selectors[4] == 4 && snapshot.selectors[5] == 5);
}

static void test_merges_mode_zero_adapter_buttons(void) {
    UsbXboxGipInputSnapshot snapshot = {0};
    const WheelAdapterInput adapter = {
        .buttons = {0x7f, 0x3f, 0},
        .mode = 0,
        .connected = true,
    };

    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 4, &adapter, false);
    assert(snapshot.buttons[0] == 0xfc);
    assert(snapshot.buttons[1] == 0x3f);

    snapshot = (UsbXboxGipInputSnapshot){.buttons = {0x10, 0xff}};
    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 6, &adapter, true);
    assert(snapshot.buttons[0] == 0xec);
    assert(snapshot.buttons[1] == 0xf0);
}

static void test_merges_mode_one_adapter_buttons(void) {
    UsbXboxGipInputSnapshot snapshot = {.buttons = {0, 0xc0}};
    const WheelAdapterInput adapter = {
        .buttons = {0xff, 0x07, 0x0c},
        .mode = 1,
        .connected = true,
    };

    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 0x15, &adapter, false);
    assert(snapshot.buttons[0] == 0xfc);
    assert(snapshot.buttons[1] == 0xcf);

    snapshot = (UsbXboxGipInputSnapshot){.buttons = {0x10, 0xff}};
    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 0x0c, &adapter, true);
    assert(snapshot.buttons[0] == 0xec);
    assert(snapshot.buttons[1] == 0xf0);
}

static void test_merges_fallback_adapter_buttons(void) {
    UsbXboxGipInputSnapshot snapshot = {
        .buttons = {0x10, 0x30},
        .packed_buttons = 3,
    };
    const WheelAdapterInput adapter = {
        .buttons = {0x3f, 0x3f, 0x0c},
        .mode = 2,
        .connected = true,
    };

    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 4, &adapter, false);
    assert(snapshot.buttons[0] == 0xfc);
    assert(snapshot.buttons[1] == 0x0f);
    assert(snapshot.packed_buttons == 0x33);

    snapshot = (UsbXboxGipInputSnapshot){.buttons = {0x10, 0xff}};
    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 9, &adapter, false);
    assert(snapshot.buttons[0] == 0x10);
    assert(snapshot.buttons[1] == 0xff);
    assert(snapshot.packed_buttons == 0);
}

static void test_ignores_unavailable_adapter_mapping(void) {
    UsbXboxGipInputSnapshot snapshot = {.buttons = {0x12, 0x34}};
    WheelAdapterInput adapter = {.buttons = {0xff, 0xff, 0xff}, .connected = true};

    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 9, &adapter, false);
    assert(snapshot.buttons[0] == 0x12 && snapshot.buttons[1] == 0x34);
    adapter.connected = false;
    usb_xbox_gip_input_merge_adapter_buttons(&snapshot, 4, &adapter, false);
    assert(snapshot.buttons[0] == 0x12 && snapshot.buttons[1] == 0x34);
}

int main(void) {
    test_maps_primary_buttons_and_alternates_packets();
    test_maps_mode_specific_extensions();
    test_maps_axes_profile_and_selectors();
    test_merges_mode_zero_adapter_buttons();
    test_merges_mode_one_adapter_buttons();
    test_merges_fallback_adapter_buttons();
    test_ignores_unavailable_adapter_mapping();
    return 0;
}
