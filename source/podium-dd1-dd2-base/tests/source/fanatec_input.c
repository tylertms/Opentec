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
    uint8_t compatibility_report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE];

    assert(!fanatec_input_encode(NULL, &state));
    assert(!fanatec_input_encode(report, NULL));
    assert(!fanatec_input_compatibility_encode(NULL, &state));
    assert(!fanatec_input_compatibility_encode(compatibility_report, NULL));
}

static void test_compatibility_encode(void) {
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
    const uint8_t expected[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE] = {
        0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xa9, 0xba,
        0xcb, 0xdc, 0xed, 0xfe, 0x0f, 0x34, 0x12, 0x45, 0x23, 0x56, 0x34,
        0x67, 0x45, 0x56, 0x67, 0x78, 0xf9, 0x89, 0x9a, 0xab, 0x09, 0x03,
    };
    uint8_t report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE];

    assert(fanatec_input_compatibility_encode(report, &state));
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_wheel_control_mapping(void) {
    fanatec_input_state state = {.accessory = {0, 0, 0, 0, 0xa0}};
    const uint8_t controls[8] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0xf6, 0x87};

    fanatec_input_apply_wheel_controls(&state, controls, true);

    assert(memcmp(state.rotary, controls, sizeof(state.rotary)) == 0);
    assert(state.accessory[0] == 0x65);
    assert(state.accessory[4] == 0xa6);
    assert(state.transfer_code == 0x87);
}

static void test_restricted_wheel_control_mapping(void) {
    fanatec_input_state state = {
        .rotary = {1, 2, 3, 4, 5},
        .accessory = {6, 7, 8, 9, 10},
    };
    const uint8_t controls[8] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87};

    fanatec_input_apply_wheel_controls(&state, controls, false);

    assert(state.rotary[0] == 0x10);
    assert(state.rotary[1] == 0x21);
    assert(state.rotary[2] == 3);
    assert(state.rotary[3] == 4);
    assert(state.rotary[4] == 5);
    assert(state.accessory[0] == 6);
    assert(state.accessory[4] == 10);
    assert(state.transfer_code == 0x87);
}

static void test_multi_position_mode_mapping(void) {
    fanatec_input_state state = {.transfer_code = 0xcf};

    fanatec_input_apply_multi_position_mode(&state, 0);
    assert(state.transfer_code == 0xcf);
    fanatec_input_apply_multi_position_mode(&state, 1);
    assert(state.transfer_code == 0xdf);
    fanatec_input_apply_multi_position_mode(&state, 2);
    assert(state.transfer_code == 0xef);
    fanatec_input_apply_multi_position_mode(&state, 7);
    assert(state.transfer_code == 0xff);
}

static void test_h_pattern_shifter_mapping(void) {
    fanatec_input_state state = {
        .button_banks = {0, 0x80, 0xff, 0, 0xff},
    };
    ShifterInputState shifter = {
        .primary_mode = SHIFTER_INPUT_H_PATTERN,
        .secondary_mode = SHIFTER_INPUT_SEQUENTIAL,
        .secondary_transition = true,
    };

    fanatec_input_apply_shifter(&state, &shifter, SHIFTER_GEAR_THIRD);

    assert(state.button_banks[1] == 0x82);
    assert(state.button_banks[2] == SHIFTER_GEAR_THIRD);
    assert(state.button_banks[4] == 0xfc);
}

static void test_sequential_shifter_mapping(void) {
    fanatec_input_state state = {
        .button_banks = {0, 0x83, 0xff, 0, 0xfc},
    };
    ShifterInputState shifter = {
        .primary_mode = SHIFTER_INPUT_SEQUENTIAL,
        .secondary_mode = SHIFTER_INPUT_SEQUENTIAL,
        .primary_transition = true,
    };

    fanatec_input_apply_shifter(&state, &shifter, SHIFTER_GEAR_FIFTH);

    assert(state.button_banks[1] == 0x83);
    assert(state.button_banks[2] == 0);
    assert(state.button_banks[4] == 0xfe);
}

int main(void) {
    test_encode();
    test_zero_state();
    test_validation();
    test_compatibility_encode();
    test_wheel_control_mapping();
    test_restricted_wheel_control_mapping();
    test_multi_position_mode_mapping();
    test_h_pattern_shifter_mapping();
    test_sequential_shifter_mapping();
    return 0;
}
