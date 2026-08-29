#include "usb/playstation_input.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

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
    assert(memcmp(report + 5, (uint8_t[]){0x58, 0x55, 0xab}, 3) == 0);
    assert(memcmp(report + 8, (uint8_t[0x23]){0}, 0x23) == 0);
    assert(memcmp(report + 0x2b,
                  (uint8_t[]){0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde, 0xc0, 0x57, 0x13},
                  11) == 0);
    assert(memcmp(report + 0x36, (uint8_t[10]){0}, 10) == 0);
}

static void test_rejects_invalid_arguments(void) {
    UsbPlaystationInputState state = {.hat = 9};
    uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE];

    assert(!usb_playstation_input_encode(0, &state));
    assert(!usb_playstation_input_encode(report, 0));
    assert(!usb_playstation_input_encode(report, &state));
}

int main(void) {
    test_maps_directional_buttons_to_hat();
    test_maps_single_clutch_axis();
    test_maps_adapter_clutch_axes();
    test_maps_dual_and_unsupported_clutch_axes();
    test_encodes_complete_input_layout();
    test_rejects_invalid_arguments();
    return 0;
}
