#include "usb/playstation_input.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define BUTTON(index) ((uint16_t)1u << (index))

static void test_maps_standard_and_legacy_buttons(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);

    UsbPlaystationButtonInput input = {
        .wheel_mode = 0x10,
        .directional_buttons = 0xb0,
        .secondary_buttons = 0x07fbu,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.hat == 8);
    assert(state.buttons == (BUTTON(0) | BUTTON(1) | BUTTON(3) | BUTTON(4) | BUTTON(5) | BUTTON(6) |
                             BUTTON(7) | BUTTON(8) | BUTTON(9) | BUTTON(10) | BUTTON(12)));
    assert(state.vendor_buttons == 0);

    input = (UsbPlaystationButtonInput){
        .wheel_mode = 0x0e,
        .directional_buttons = 0xe0,
        .secondary_buttons = 0x01fbu,
        .auxiliary_history = 0x60,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.buttons == 0x1fffu);
}

static void test_maps_alternate_button_modes(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);

    UsbPlaystationButtonInput input = {
        .wheel_mode = 0x0f,
        .directional_buttons = 0xd0,
        .secondary_buttons = 0x05d6,
        .adapter_mode = 2,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.buttons == (BUTTON(1) | BUTTON(2) | BUTTON(3) | BUTTON(6) | BUTTON(7) | BUTTON(8) |
                             BUTTON(9) | BUTTON(11) | BUTTON(12)));

    input = (UsbPlaystationButtonInput){
        .wheel_mode = 0x11,
        .secondary_buttons = 0x0ef6,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.buttons == (BUTTON(1) | BUTTON(6) | BUTTON(7) | BUTTON(8) | BUTTON(9) |
                             BUTTON(10) | BUTTON(11) | BUTTON(12)));
}

static void test_maps_both_adapter_button_layouts(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);

    UsbPlaystationButtonInput input = {
        .wheel_mode = 4,
        .directional_buttons = 0xa0,
        .secondary_buttons = 0x04fbu,
        .adapter_buttons = {0xf0, 0x3f, 0x0c},
        .adapter_connected = true,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.buttons == 0x17ffu);

    input.adapter_mode = 1;
    input.adapter_buttons[1] = 0x7f;
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.buttons == 0x1fffu);
}

static void test_suppresses_hat_and_system_button(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);

    UsbPlaystationButtonInput input = {
        .wheel_mode = 5,
        .directional_buttons = 8,
        .secondary_buttons = 0x0200,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.hat == 4);
    assert((state.buttons & BUTTON(12)) != 0);

    input.hat_suppressed = true;
    input.system_button_suppressed = true;
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.hat == 8);
    assert((state.buttons & BUTTON(12)) == 0);

    input.hat_suppressed = false;
    input.system_button_suppressed = false;
    input.secondary_buttons = 0x2000;
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.hat == 8);
    assert((state.buttons & BUTTON(12)) == 0);
}

static void test_clears_secondary_hat_source_before_mapping(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);

    UsbPlaystationButtonInput input = {
        .wheel_mode = 0x0a,
        .secondary_buttons = 0x2300,
        .hat_suppressed = true,
        .system_button_suppressed = true,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.hat == 8);
    assert((state.buttons & BUTTON(1)) == 0);
    assert((state.buttons & BUTTON(12)) != 0);

    input.hat_suppressed = false;
    input.system_button_suppressed = false;
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert(state.hat == 8);
    assert((state.buttons & BUTTON(1)) == 0);
    assert((state.buttons & BUTTON(12)) != 0);
}

static void test_preserves_mode_system_reassertion_after_preclear(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);

    UsbPlaystationButtonInput input = {
        .wheel_mode = 0x0a,
        .secondary_buttons = 0x0300,
        .hat_suppressed = true,
        .system_button_suppressed = true,
    };
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert((state.buttons & BUTTON(1)) == 0);
    assert((state.buttons & BUTTON(12)) != 0);

    input.wheel_mode = 1;
    input.secondary_buttons = 0x0a00;
    assert(usb_playstation_input_map_buttons(&mapper, &input, 0, &state));
    assert((state.buttons & BUTTON(1)) == 0);
    assert((state.buttons & BUTTON(12)) != 0);
}

static void test_holds_mode_twelve_system_button(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);
    UsbPlaystationButtonInput input = {
        .wheel_mode = 0x0c,
        .secondary_buttons = 0x0080,
    };

    assert(usb_playstation_input_map_buttons(&mapper, &input, 100, &state));
    assert((state.buttons & BUTTON(12)) == 0);
    assert(usb_playstation_input_map_buttons(&mapper, &input, 3099, &state));
    assert((state.buttons & BUTTON(12)) == 0);
    assert(usb_playstation_input_map_buttons(&mapper, &input, 3100, &state));
    assert((state.buttons & BUTTON(12)) != 0);

    input.secondary_buttons = 0;
    assert(usb_playstation_input_map_buttons(&mapper, &input, 3101, &state));
    assert((state.buttons & BUTTON(12)) == 0);
    assert(!mapper.system_button_hold_active);
}

static void test_rejects_invalid_button_mapping_arguments(void) {
    UsbPlaystationInputMapper mapper;
    UsbPlaystationButtonInput input = {0};
    UsbPlaystationInputState state = {0};
    usb_playstation_input_mapper_init(&mapper);

    assert(!usb_playstation_input_map_buttons(0, &input, 0, &state));
    assert(!usb_playstation_input_map_buttons(&mapper, 0, 0, &state));
    assert(!usb_playstation_input_map_buttons(&mapper, &input, 0, 0));
}

static void test_maps_directional_buttons_to_hat(void) {
    static const uint8_t expected[16] = {8, 2, 6, 8, 4, 3, 5, 0, 0, 1, 7, 0, 8, 0, 2, 5};
    for (uint8_t input = 0; input < 16; input++) {
        uint8_t encoded = (uint8_t)(((input >> 3) & 0x01u) | ((input & 0x04u) << 1) |
                                    (input & 0x02u) | ((input & 0x01u) << 2));
        assert(usb_playstation_input_map_hat(encoded) == expected[input]);
        assert(usb_playstation_input_map_hat((uint8_t)(encoded | 0xf0u)) == expected[input]);
    }
}

static void test_maps_single_clutch_axis(void) {
    UsbPlaystationClutchInput input = {
        .wheel_mode = 9,
        .wheel_axes = {0x21, 0x32},
        .wheel_axis_enabled = true,
    };
    uint8_t axes[2];

    usb_playstation_input_map_clutch(axes, &input);
    assert(memcmp(axes, (uint8_t[]){0x5e, 0x80}, 2) == 0);
    input.wheel_axis_enabled = false;
    usb_playstation_input_map_clutch(axes, &input);
    assert(memcmp(axes, (uint8_t[]){0x80, 0x80}, 2) == 0);

    const uint8_t modes[] = {11, 28, 29};
    input.wheel_axis_enabled = true;
    for (uint8_t mode = 0; mode < sizeof(modes); mode++) {
        input.wheel_mode = modes[mode];
        usb_playstation_input_map_clutch(axes, &input);
        assert(memcmp(axes, (uint8_t[]){0x5e, 0x80}, 2) == 0);
    }
}

static void test_maps_adapter_clutch_axes(void) {
    const uint8_t modes[] = {4, 6, 12, 21};
    UsbPlaystationClutchInput input = {
        .wheel_axes = {0x11, 0x22},
        .adapter_axes = {0x33, 0x44},
        .adapter_connected = true,
    };
    uint8_t axes[2];

    for (uint8_t mode = 0; mode < sizeof(modes); mode++) {
        input.wheel_mode = modes[mode];
        usb_playstation_input_map_clutch(axes, &input);
        assert(memcmp(axes, (uint8_t[]){0x44, 0xcc}, 2) == 0);
    }
    input.adapter_connected = false;
    usb_playstation_input_map_clutch(axes, &input);
    assert(memcmp(axes, (uint8_t[]){0x80, 0x80}, 2) == 0);
    input.paddle_mode = 4;
    usb_playstation_input_map_clutch(axes, &input);
    assert(memcmp(axes, (uint8_t[]){0x6e, 0xa2}, 2) == 0);
}

static void test_maps_dual_and_unsupported_clutch_axes(void) {
    const uint8_t modes[] = {1, 2, 3, 10, 14, 15, 19, 20, 22, 23};
    UsbPlaystationClutchInput input = {.wheel_axes = {0x11, 0x22}};
    uint8_t axes[2];

    for (uint8_t mode = 0; mode < sizeof(modes); mode++) {
        input.wheel_mode = modes[mode];
        usb_playstation_input_map_clutch(axes, &input);
        assert(memcmp(axes, (uint8_t[]){0x6e, 0xa2}, 2) == 0);
    }
    input.wheel_mode = 8;
    usb_playstation_input_map_clutch(axes, &input);
    assert(memcmp(axes, (uint8_t[]){0x80, 0x80}, 2) == 0);
    usb_playstation_input_map_clutch(axes, 0);
    assert(memcmp(axes, (uint8_t[]){0x80, 0x80}, 2) == 0);
    usb_playstation_input_map_clutch(0, &input);
}

static void test_encodes_complete_input_layout(void) {
    UsbPlaystationInputState state = {
        .clutch_axes = {0x7f, 0x80},
        .hat = 8,
        .buttons = 0x3555,
        .vendor_buttons = 0x2a,
        .steering = 0x1234,
        .pedals = {0x5678, 0x9abc, 0xdef0},
        .wheel_hat = 0x81,
        .auxiliary_axis = 0x1357,
    };
    uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE];

    assert(usb_playstation_input_encode(report, &state));
    assert(report[0] == 1);
    assert(memcmp(report + 1, (uint8_t[]){0x80, 0x80, 0x7f, 0x80}, 4) == 0);
    assert(memcmp(report + 5, (uint8_t[]){0x58, 0x55, 0x01}, 3) == 0);
    assert(memcmp(report + 8, (uint8_t[0x23]){0}, 0x23) == 0);
    assert(memcmp(report + 0x2b,
                  (uint8_t[]){0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde, 0xc0, 0x57, 0x13},
                  11) == 0);
    assert(memcmp(report + 0x36, (uint8_t[10]){0}, 10) == 0);
}

static void test_encodes_local_h_pattern_hat(void) {
    UsbPlaystationInputState state = {.hat = 8, .wheel_hat = 0x10};
    uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE];

    assert(usb_playstation_input_encode(report, &state));
    assert(report[0x33] == 0x08);
}

static void test_rejects_invalid_arguments(void) {
    UsbPlaystationInputState state = {.hat = 9};
    uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE];

    assert(!usb_playstation_input_encode(0, &state));
    assert(!usb_playstation_input_encode(report, 0));
    assert(!usb_playstation_input_encode(report, &state));
}

int main(void) {
    test_maps_standard_and_legacy_buttons();
    test_maps_alternate_button_modes();
    test_maps_both_adapter_button_layouts();
    test_suppresses_hat_and_system_button();
    test_clears_secondary_hat_source_before_mapping();
    test_preserves_mode_system_reassertion_after_preclear();
    test_holds_mode_twelve_system_button();
    test_rejects_invalid_button_mapping_arguments();
    test_maps_directional_buttons_to_hat();
    test_maps_single_clutch_axis();
    test_maps_adapter_clutch_axes();
    test_maps_dual_and_unsupported_clutch_axes();
    test_encodes_complete_input_layout();
    test_encodes_local_h_pattern_hat();
    test_rejects_invalid_arguments();
    return 0;
}
