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
        .encoder_position = -7,
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
        .encoder_position = -7,
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
    fanatec_input_state state = {
        .accessory = {0, 0, 0, 0, 0xaf},
        .transfer_code = 0x91,
    };
    const uint8_t controls[8] = {0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0xf6, 0x87};

    fanatec_input_apply_wheel_controls(&state, controls, true);

    assert(memcmp(state.rotary, controls, sizeof(state.rotary)) == 0);
    assert(state.accessory[0] == 0x65);
    assert(state.accessory[4] == 0xaf);
    assert(state.transfer_code == 0x91);
}

static void test_restricted_wheel_control_mapping(void) {
    fanatec_input_state state = {
        .rotary = {1, 2, 3, 4, 5},
        .accessory = {6, 7, 8, 9, 10},
        .transfer_code = 0x91,
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
    assert(state.transfer_code == 0x91);
}

static void test_wheel_accessory_mapping(void) {
    fanatec_input_state state = {.accessory = {0, 0, 0, 0, 0xa5}};

    fanatec_input_apply_wheel_accessory(&state, 0xf6);

    assert(state.accessory[4] == 0xa6);
}

static void test_multi_position_mode_mapping(void) {
    fanatec_input_state state = {.accessory = {0, 0, 0, 0, 0xcf}, .transfer_code = 0x87};

    fanatec_input_apply_multi_position_mode(&state, 0);
    assert(state.accessory[4] == 0xcf);
    fanatec_input_apply_multi_position_mode(&state, 1);
    assert(state.accessory[4] == 0xdf);
    fanatec_input_apply_multi_position_mode(&state, 2);
    assert(state.accessory[4] == 0xef);
    fanatec_input_apply_multi_position_mode(&state, 7);
    assert(state.accessory[4] == 0xff);
    assert(state.transfer_code == 0x87);
}

static void test_multi_position_encoder_mapping(void) {
    fanatec_input_state state = {.rotary = {0xff, 0xff, 0xff, 0xff, 0xff}};
    const fanatec_multi_position_input input = {
        .channels = {{.event = 1, .active = true},
                     {.event = 2, .active = true},
                     {.event = 1, .active = true}},
    };
    const uint8_t expected[FANATEC_INPUT_ROTARY_BYTES] = {1, 0x20, 0, 0x10, 0};

    fanatec_input_apply_multi_position_rotaries(&state, 0, &input);

    assert(memcmp(state.rotary, expected, sizeof(expected)) == 0);
}

static void test_multi_position_pulse_mapping(void) {
    fanatec_input_state state = {.rotary = {0xff, 0xff, 0xff, 0xff, 0xff}};
    const fanatec_multi_position_input input = {
        .channels = {{.position = 1, .active = true},
                     {.position = 5, .event = 2, .active = true},
                     {.position = 12, .event = 1}},
    };
    const uint8_t expected[FANATEC_INPUT_ROTARY_BYTES] = {0, 0, 1, 0, 0};

    fanatec_input_apply_multi_position_rotaries(&state, 1, &input);

    assert(memcmp(state.rotary, expected, sizeof(expected)) == 0);
}

static void test_multi_position_constant_mapping(void) {
    fanatec_input_state state = {0};
    const fanatec_multi_position_input input = {
        .channels = {{.position = 12, .active = true},
                     {.position = 4, .active = true},
                     {.position = 9, .active = true}},
    };
    const uint8_t expected[FANATEC_INPUT_ROTARY_BYTES] = {0, 0x88, 0, 0, 0x10};

    fanatec_input_apply_multi_position_rotaries(&state, 2, &input);

    assert(memcmp(state.rotary, expected, sizeof(expected)) == 0);
}

static void test_multi_position_remapped_selector_layout(void) {
    fanatec_input_state state = {0};
    const fanatec_multi_position_input input = {
        .channels = {{.position = 1, .active = true},
                     {.position = 5, .active = true},
                     {.position = 12, .active = true}},
        .remap_selectors = true,
    };
    const uint8_t expected[FANATEC_INPUT_ROTARY_BYTES] = {0, 0x11, 0, 0, 8};

    fanatec_input_apply_multi_position_rotaries(&state, 2, &input);

    assert(memcmp(state.rotary, expected, sizeof(expected)) == 0);
}

static void test_unsupported_multi_position_mode_clears_rotaries(void) {
    fanatec_input_state state = {.rotary = {1, 2, 3, 4, 5}};
    const fanatec_multi_position_input input = {
        .channels = {{.position = 1, .event = 1, .active = true}},
    };
    const uint8_t expected[FANATEC_INPUT_ROTARY_BYTES] = {0};

    fanatec_input_apply_multi_position_rotaries(&state, 3, &input);

    assert(memcmp(state.rotary, expected, sizeof(expected)) == 0);
}

static void test_h_pattern_shifter_mapping(void) {
    fanatec_input_state state = {
        .button_banks = {0, 0x80, 0xff, 0, 0xff},
        .status_flags = 0xff,
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
    assert(state.status_flags == 0xfe);
}

static void test_sequential_shifter_mapping(void) {
    fanatec_input_state state = {
        .button_banks = {0, 0x83, 0xff, 0, 0xfc},
        .status_flags = 0xa0,
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
    assert(state.status_flags == 0xa1);
}

static void test_thermal_effect_limit_status(void) {
    fanatec_input_state state = {.status_flags = 0xa1};

    fanatec_input_apply_thermal_limit(&state, true);
    assert(state.status_flags == 0xb1);

    fanatec_input_apply_thermal_limit(&state, false);
    assert(state.status_flags == 0xa1);
}

static void test_wheel_calibration_status(void) {
    fanatec_input_state state = {.status_flags = 0x91};

    fanatec_input_apply_wheel_calibration(&state, true);
    assert(state.status_flags == 0xd1);

    fanatec_input_apply_wheel_calibration(&state, false);
    assert(state.status_flags == 0x91);
}

static void test_wheel_input_capability_status(void) {
    fanatec_input_state state = {.status_flags = 0x51};

    fanatec_input_apply_wheel_input_capability(&state, true);
    assert(state.status_flags == 0xd1);

    fanatec_input_apply_wheel_input_capability(&state, false);
    assert(state.status_flags == 0x51);
}

int main(void) {
    test_encode();
    test_zero_state();
    test_validation();
    test_compatibility_encode();
    test_wheel_control_mapping();
    test_restricted_wheel_control_mapping();
    test_wheel_accessory_mapping();
    test_multi_position_mode_mapping();
    test_multi_position_encoder_mapping();
    test_multi_position_pulse_mapping();
    test_multi_position_constant_mapping();
    test_multi_position_remapped_selector_layout();
    test_unsupported_multi_position_mode_clears_rotaries();
    test_h_pattern_shifter_mapping();
    test_sequential_shifter_mapping();
    test_thermal_effect_limit_status();
    test_wheel_calibration_status();
    test_wheel_input_capability_status();
    return 0;
}
