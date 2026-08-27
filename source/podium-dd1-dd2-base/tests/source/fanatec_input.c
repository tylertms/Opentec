#include <assert.h>
#include <string.h>
#include <usb/fanatec_input.h>

static void test_encode(void) {
    const fanatec_input_state state = {
        .button_banks = {0x10, 0x21, 0x32, 0x43, 0x54},
        .rotary = {0x65, 0x76, 0x87, 0x98, 0xa9},
        .accessory = {0xba, 0xcb, 0xdc, 0xed, 0xfe},
        .steering = 0x1234,
        .pedals = {0x2345, 0x3456, 0x4567},
        .clutch_paddles = {0x56, 0x67},
        .auxiliary_pedal = 0x78,
        .encoder_delta = -7,
        .transfer_code = 0x0f,
        .status_flags = 0x89,
        .wheel_mode = 0x9a,
        .axis_limit = 0xab,
    };
    const uint8_t expected[FANATEC_INPUT_REPORT_SIZE] = {
        0x01, 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xa9, 0xba,
        0xcb, 0xdc, 0xed, 0xfe, 0x0f, 0x34, 0x12, 0x45, 0x23, 0x56, 0x34, 0x67,
        0x45, 0x56, 0x67, 0x78, 0xf9, 0x89, 0x9a, 0xab, 0x09, 0x03,
    };
    uint8_t report[FANATEC_INPUT_REPORT_SIZE];

    assert(fanatec_input_encode(report, &state));
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_zero_state(void) {
    const fanatec_input_state state = {0};
    uint8_t report[FANATEC_INPUT_REPORT_SIZE];
    uint8_t index;

    assert(fanatec_input_encode(report, &state));
    assert(report[0] == FANATEC_INPUT_REPORT_ID);
    assert(report[32] == 9);
    assert(report[33] == 3);
    for (index = 1; index < 32; ++index) {
        assert(report[index] == 0);
    }
}

static void test_validation(void) {
    fanatec_input_state state = {0};
    uint8_t report[FANATEC_INPUT_REPORT_SIZE];

    assert(!fanatec_input_encode(NULL, &state));
    assert(!fanatec_input_encode(report, NULL));
}

int main(void) {
    test_encode();
    test_zero_state();
    test_validation();
    return 0;
}
