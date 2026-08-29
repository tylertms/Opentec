#include "usb/playstation_input.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

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
    test_encodes_complete_input_layout();
    test_rejects_invalid_arguments();
    return 0;
}
