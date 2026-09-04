#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/input_report.h"

static const UsbInputReportState state = {
    .fanatec =
        {
            .button_banks = {0x10, 0x21, 0x32, 0x43, 0x54},
            .rotary = {0x65, 0x76, 0x87, 0x98, 0xa9},
            .accessory = {0xba, 0xcb, 0xdc, 0xed, 0xfe},
            .steering = 0x1234,
            .pedals = {0x2345, 0x3456, 0x4567},
            .clutch_paddles = {0x56, 0x67},
            .auxiliary_pedal = 0x78,
            .encoder_position = -7,
            .transfer_code = 0x0f,
            .status_flags = 0x89,
            .wheel_mode = 0x9a,
            .axis_limit = 0xab,
        },
    .logitech =
        {
            .steering = 0x9234,
            .buttons = 0x5abcde,
            .hat = 7,
            .axes = {0x11, 0x22, 0x33},
        },
};

static void test_fanatec_modes(void) {
    const uint8_t expected[FANATEC_INPUT_REPORT_SIZE] = {
        0x01, 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xa9, 0xba,
        0xcb, 0xdc, 0xed, 0xfe, 0x0f, 0x34, 0x12, 0x45, 0x23, 0x56, 0x34, 0x67,
        0x45, 0x56, 0x67, 0x78, 0xf9, 0x89, 0x9a, 0xab, 0x09, 0x03,
    };
    uint8_t report[USB_INPUT_REPORT_MAX_SIZE];

    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_FANATEC, report, &state) ==
           FANATEC_INPUT_REPORT_SIZE);
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_invalid_command_reports_direct_drive_mode(void) {
    UsbInputReportState invalid_state = state;
    invalid_state.fanatec.wheel_mode =
        fanatec_input_report_mode(state.fanatec.wheel_mode, true);
    uint8_t report[USB_INPUT_REPORT_MAX_SIZE];

    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_FANATEC, report, &invalid_state) ==
           FANATEC_INPUT_REPORT_SIZE);
    assert(report[30] == FANATEC_INPUT_DIRECT_DRIVE_MODE);
}

static void test_fanatec_compatibility_mode(void) {
    const uint8_t expected[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE] = {
        0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xa9, 0xba,
        0xcb, 0xdc, 0xed, 0xfe, 0x0f, 0x34, 0x12, 0x45, 0x23, 0x56, 0x34,
        0x67, 0x45, 0x56, 0x67, 0x78, 0xf9, 0x89, 0x9a, 0xab, 0x09, 0x03,
    };
    uint8_t report[USB_INPUT_REPORT_MAX_SIZE];

    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY, report, &state) ==
           FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE);
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_driving_force_ex_mode(void) {
    const uint8_t expected[LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE] = {
        0x48, 0x7a, 0x33, 0x11, 0x07, 0x22, 0x33,
    };
    uint8_t report[USB_INPUT_REPORT_MAX_SIZE];

    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_DRIVING_FORCE_EX, report, &state) ==
           LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE);
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_driving_force_pro_mode(void) {
    const uint8_t expected[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE] = {
        0x8d, 0xa4, 0x00, 0x37, 0x7f, 0x11, 0xa5, 0x5a,
    };
    uint8_t report[USB_INPUT_REPORT_MAX_SIZE] = {0};
    report[6] = 0xa5;
    report[7] = 0x5a;

    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO, report, &state) ==
           LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE);
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_g27_mode(void) {
    const uint8_t expected[LOGITECH_G27_REPORT_SIZE] = {
        0xe7, 0xcd, 0xab, 0x35, 0x92, 0x11, 0x22, 0x33, 0x80, 0x80, 0x03,
    };
    uint8_t report[USB_INPUT_REPORT_MAX_SIZE];

    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_G27, report, &state) ==
           LOGITECH_G27_REPORT_SIZE);
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_validation(void) {
    uint8_t report[USB_INPUT_REPORT_MAX_SIZE];

    assert(usb_input_report_encode((UsbInputReportMode)5, report, &state) == 0);
    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_FANATEC, NULL, &state) == 0);
    assert(usb_input_report_encode(USB_INPUT_REPORT_MODE_FANATEC, report, NULL) == 0);
}

int main(void) {
    test_fanatec_modes();
    test_invalid_command_reports_direct_drive_mode();
    test_fanatec_compatibility_mode();
    test_driving_force_ex_mode();
    test_driving_force_pro_mode();
    test_g27_mode();
    test_validation();
    return 0;
}
