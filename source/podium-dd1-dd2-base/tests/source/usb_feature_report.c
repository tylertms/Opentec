#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "profile/bank.h"
#include "usb/feature_report.h"

static void test_status_report(void) {
    uint8_t output[USB_DEVICE_REPORT_SIZE];
    UsbFeatureReport31State state = {
        .status = 0x1234,
        .wheel_mode = 0x1c,
        .pedal_active = 0x10,
        .auxiliary_profile = 1,
        .axis_modes = {2, 2},
        .transfer_code = 0x42,
        .rotary_mode = 2,
        .pedal_legacy = true,
        .pedal_io_active = true,
        .pedal_handshake_active = true,
        .resistance_active = true,
        .pedal_calibration_active = true,
        .wheel_calibration_available = true,
        .wheel_axis_report_enabled = true,
        .adapter_connected = true,
    };
    usb_feature_report_31_encode(&state, output);
    assert(output[0] == 0x31 && output[1] == 1 && output[2] == 0xff);
    assert(output[3] == 0x1c && output[4] == 0x10 && output[5] == 1 && output[6] == 2);
    assert(output[7] == 3 && output[8] == 9 && output[9] == 0x42 && output[10] == 2);
    assert(output[11] == 1 && output[12] == 0x34 && output[13] == 0x12);
}

static void test_tuning_report(void) {
    uint8_t output[USB_DEVICE_REPORT_SIZE];
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.active_slot = 2;
    bank.selected_slot = 4;
    bank.slots[2].force_feedback_strength = 73;
    usb_feature_report_32_encode(&bank, true, output);
    assert(output[0] == 0x32 && output[1] == 3 && output[2] == 5 && output[3] == 1);
    assert(output[6] == 73);
}

static void test_rotary_and_fourth_pulse_report(void) {
    uint8_t output[USB_DEVICE_REPORT_SIZE];
    UsbFeatureReport33State state = {
        .positions = {1, 12, 5},
        .events = {1, -1, 1},
        .pulse_directions = {1, -1, 1, -1},
        .secondary_buttons = 0x4800,
        .control_extended = {2, 0},
        .auxiliary_report = {0x70, 0, 0x0c},
        .wheel_mode = 0x1c,
        .rotary_mode = 2,
    };
    usb_feature_report_33_encode(&state, output);
    assert(output[0] == 0x33);
    assert(output[1] == 1 && output[2] == 0 && output[3] == 0x80);
    assert(output[5] == 0x66);
    assert(output[6] == 0x10 && output[7] == 0);
    assert(output[12] == 0x25);
    assert(output[13] == 0xa0 && output[14] == 0x43 && output[15] == 0);
}

static void test_rotary_supported_mode_gate(void) {
    assert(usb_feature_report_33_supports_rotary(0x15));
    assert(!usb_feature_report_33_supports_rotary(0x18));
}

static void test_page_report(void) {
    uint8_t output[USB_DEVICE_REPORT_SIZE];
    usb_feature_report_36_encode(6, output);
    assert(output[0] == 0x36 && output[1] == 2 && output[2] == 6);
}

int main(void) {
    test_status_report();
    test_tuning_report();
    test_rotary_and_fourth_pulse_report();
    test_rotary_supported_mode_gate();
    test_page_report();
    return 0;
}
