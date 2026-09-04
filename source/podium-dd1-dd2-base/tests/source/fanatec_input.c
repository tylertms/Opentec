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

static void test_bite_point_update(void) {
    fanatec_input_state state = {
        .wheel_mode = 0xfe,
        .axis_limit = 0x17,
    };
    uint8_t report[FANATEC_INPUT_REPORT_SIZE];
    uint8_t compatibility_report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE];

    fanatec_input_apply_bite_point_update(&state, 73);
    assert(fanatec_input_encode(report, &state));
    assert(report[30] == 0xff);
    assert(report[31] == 2);
    assert(report[32] == 73);
    assert(report[33] == 0);

    assert(fanatec_input_compatibility_encode(compatibility_report, &state));
    assert(compatibility_report[29] == 0xfe);
    assert(compatibility_report[30] == 0x17);
    assert(compatibility_report[31] == 9);
    assert(compatibility_report[32] == 3);
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

static void test_invalid_command_report_mode(void) {
    assert(fanatec_input_report_mode(0x10, false) == 0x10);
    assert(fanatec_input_report_mode(0x10, true) == FANATEC_INPUT_DIRECT_DRIVE_MODE);
    assert(fanatec_input_report_mode(FANATEC_INPUT_DIRECT_DRIVE_MODE, true) ==
           FANATEC_INPUT_DIRECT_DRIVE_MODE);
}

static void test_standard_profile_selector_masks_packed_rotary_low_nibble(void) {
    fanatec_input_source source = {
        .packed_rotary_positions = 0xff,
        .mode = 0x10,
        .protocol_active = true,
        .profile_selector_held = true,
    };
    fanatec_input_state masked = {0};
    fanatec_input_pipeline_map(&masked, &source);

    source.packed_rotary_positions = 0xf0;
    source.profile_selector_held = false;
    fanatec_input_state expected = {0};
    fanatec_input_pipeline_map(&expected, &source);
    assert(memcmp(masked.accessory, expected.accessory, sizeof(masked.accessory)) == 0);

    source.packed_rotary_positions = 0xff;
    fanatec_input_state unmasked = {0};
    fanatec_input_pipeline_map(&unmasked, &source);
    assert(memcmp(masked.accessory, unmasked.accessory, sizeof(masked.accessory)) != 0);

    source.mode = 0x0e;
    source.profile_selector_held = true;
    fanatec_input_state legacy = {0};
    fanatec_input_pipeline_map(&legacy, &source);
    source.profile_selector_held = false;
    fanatec_input_state expected_legacy = {0};
    fanatec_input_pipeline_map(&expected_legacy, &source);
    assert(memcmp(legacy.accessory, expected_legacy.accessory, sizeof(legacy.accessory)) == 0);
}

static void test_official_source_history(void) {
    fanatec_input_pipeline_state pipeline;
    fanatec_input_source source;

    fanatec_input_pipeline_init(&pipeline);
    for (uint8_t sample = 0; sample < FANATEC_INPUT_HISTORY_DEPTH; sample++) {
        source = (fanatec_input_source){
            .buttons = {0xff, 0xff, 0xff},
            .secondary_buttons = 0xff,
            .packed_rotary_positions = 0xff,
            .accessory = 0xff,
            .mode = 0x10,
        };
        fanatec_input_pipeline_filter(&pipeline, &source);
        if (sample + 1 < FANATEC_INPUT_HISTORY_DEPTH) {
            assert(memcmp(source.buttons, (uint8_t[3]){0, 0, 0}, 3) == 0);
            assert(source.secondary_buttons == 0);
            assert(source.packed_rotary_positions == 0);
            assert(source.accessory == 0);
        } else {
            assert(memcmp(source.buttons, (uint8_t[3]){0xff, 0xff, 0xff}, 3) == 0);
            assert(source.secondary_buttons == 0xff);
            assert(source.packed_rotary_positions == 0xff);
            assert(source.accessory == 0xff);
        }
    }

    source = (fanatec_input_source){
        .buttons = {0xff, 0xff, 0xff},
        .secondary_buttons = 0xa5,
        .packed_rotary_positions = 0x5a,
        .accessory = 0x3c,
        .mode = 0x0e,
    };
    fanatec_input_pipeline_filter(&pipeline, &source);
    assert(source.secondary_buttons == 0xa5);
    assert(source.packed_rotary_positions == 0x5a);
    assert(source.accessory == 0x3c);
}

static void test_official_first_five_mapping_and_axes(void) {
    const fanatec_input_source source = {
        .buttons = {0x80, 0x01, 0x00},
        .hat = 0x05,
        .secondary_buttons = 0x01,
        .mode = 0x10,
        .auxiliary_flags = 0xa0,
        .protocol_active = true,
        .calibration_available = true,
        .axis_report_enabled = true,
    };
    fanatec_input_state state = {0};

    fanatec_input_pipeline_map(&state, &source);

    assert(memcmp(state.button_banks, (uint8_t[5]){0x88, 0x01, 0x05, 0x00, 0x00}, 5) == 0);
    assert(state.accessory[1] == 0xa0);
    assert(state.accessory[2] == 0x08);
    assert(state.accessory[3] == 0x00);
    assert(state.status_flags == 0xc0);
    assert(state.wheel_mode == 0x10);
}

static void test_legacy_mode_maps_auxiliary_high_nibble(void) {
    const fanatec_input_source active_source = {
        .auxiliary_flags = 0xa7,
        .mode = 0x0e,
        .protocol_active = true,
    };
    fanatec_input_state state = {.accessory = {0, 0x05}};

    fanatec_input_pipeline_map(&state, &active_source);
    assert(state.accessory[1] == 0xa0);

    const fanatec_input_source inactive_source = {
        .auxiliary_flags = 0xa7,
        .mode = 0x0e,
    };
    state = (fanatec_input_state){.accessory = {0, 0x05}};

    fanatec_input_pipeline_map(&state, &inactive_source);
    assert(state.accessory[1] == 0xa5);
}

static void test_protocol_active_clears_extended_fields(void) {
    const fanatec_input_source source = {
        .rotary_positions = {0x12, 0x34},
        .extended_buttons = {0x56, 0x78, 0x9a, 0xbc},
        .accessory = 0x0d,
        .mode = 0x01,
        .protocol_active = true,
    };
    fanatec_input_state state = {
        .rotary = {0xa1, 0xa2, 0xa3, 0xa4, 0xa5},
        .accessory = {0xb1, 0xb2, 0xb3, 0xb4, 0xb5},
    };
    const uint8_t expected_rotary[FANATEC_INPUT_ROTARY_BYTES] = {0x12, 0x34, 0, 0x78, 0};
    const uint8_t expected_accessory[FANATEC_INPUT_ACCESSORY_BYTES] = {0, 0, 0, 0, 0x0d};

    fanatec_input_pipeline_map(&state, &source);

    assert(memcmp(state.rotary, expected_rotary, sizeof(expected_rotary)) == 0);
    assert(memcmp(state.accessory, expected_accessory, sizeof(expected_accessory)) == 0);

    uint8_t report[FANATEC_INPUT_REPORT_SIZE];
    assert(fanatec_input_encode(report, &state));
    assert(memcmp(report + 6, expected_rotary, sizeof(expected_rotary)) == 0);
    assert(memcmp(report + 11, expected_accessory, sizeof(expected_accessory)) == 0);
}

static void test_auxiliary_transfer_code_reaches_native_report_byte(void) {
    const fanatec_input_source source = {
        .transfer_code = 0x3f,
        .mode = 0x10,
        .protocol_active = true,
    };
    fanatec_input_state state = {0};
    uint8_t report[FANATEC_INPUT_REPORT_SIZE];
    uint8_t compatibility_report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE];

    fanatec_input_pipeline_map(&state, &source);
    assert(state.transfer_code == 0x3f);
    assert(fanatec_input_encode(report, &state));
    assert(report[16] == 0x3f);
    assert(fanatec_input_compatibility_encode(compatibility_report, &state));
    assert(compatibility_report[15] == 0x3f);
}

static void test_production_pipeline_first_five_report(void) {
    fanatec_input_pipeline_state pipeline;
    fanatec_input_state state = {0};
    const fanatec_input_source source = {
        .buttons = {0x80, 0x01, 0x00},
        .hat = 0x05,
        .secondary_buttons = 0x01,
        .mode = 0x10,
        .auxiliary_flags = 0xa0,
        .protocol_active = true,
        .calibration_available = true,
        .axis_report_enabled = true,
    };
    uint8_t report[FANATEC_INPUT_REPORT_SIZE];

    fanatec_input_pipeline_init(&pipeline);
    for (uint8_t sample = 0; sample < FANATEC_INPUT_HISTORY_DEPTH; sample++) {
        fanatec_input_pipeline_apply(&pipeline, &state, &source);
    }

    assert(fanatec_input_encode(report, &state));
    assert(memcmp(report + 1, (uint8_t[5]){0x88, 0x01, 0x05, 0x00, 0x00}, 5) == 0);
}

static void test_primary_third_button_bank_mapping(void) {
    const fanatec_input_source source = {
        .buttons = {0, 0, 0xff},
        .hat = 0x05,
        .mode = 0x10,
        .protocol_active = true,
    };
    fanatec_input_state state = {0};

    fanatec_input_pipeline_map(&state, &source);

    assert(state.button_banks[2] == 0x05);
    assert(state.button_banks[3] == 0x92);
}

static void test_default_auxiliary_button_source(void) {
    fanatec_input_source source = {
        .auxiliary_buttons = 0x03,
        .mode = 0x01,
        .protocol_active = true,
    };
    fanatec_input_state state = {0};

    fanatec_input_pipeline_map(&state, &source);
    assert(state.button_banks[4] == 0x0c);

    source.auxiliary_buttons = 0;
    source.secondary_buttons = 0x0003;
    state = (fanatec_input_state){0};
    fanatec_input_pipeline_map(&state, &source);
    assert(state.button_banks[4] == 0);
}

static void test_adapter_auxiliary_merges(void) {
    fanatec_input_source source = {
        .auxiliary_buttons = 0x03,
        .adapter_connected = true,
        .adapter_mode = 0,
        .protocol_active = true,
    };
    fanatec_input_state state = {0};

    fanatec_input_pipeline_map(&state, &source);
    assert(state.button_banks[4] == 0x0c);

    source.auxiliary_buttons = 0x01;
    source.adapter_mode = 1;
    source.buttons[2] = 0x04;
    state = (fanatec_input_state){0};
    fanatec_input_pipeline_map(&state, &source);
    assert(state.button_banks[1] == 0x80);
    assert((state.button_banks[3] & 0x20) == 0);
}

static void test_pulse_status_mapping(void) {
    const fanatec_input_source source = {
        .mode = 0x1b,
        .pulse_flags = {0x0f, 0x03, 0, 0},
        .protocol_active = true,
    };
    fanatec_input_state state = {0};

    fanatec_input_pipeline_map(&state, &source);

    assert(state.button_banks[3] == 0x41);
    assert(state.button_banks[4] == 0xc0);
    assert(state.accessory[3] == 0xc0);
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

static void test_quaternary_rotary_event_mapping(void) {
    fanatec_input_state state = {.accessory = {0xa5, 0, 0, 0, 0}};

    fanatec_input_apply_quaternary_rotary_event(&state, 2);

    assert(state.accessory[0] == 2);
}

static void test_wheel_accessory_mapping(void) {
    fanatec_input_state state = {.accessory = {0, 0, 0, 0, 0xa5}};

    fanatec_input_apply_wheel_accessory(&state, 0xf6);

    assert(state.accessory[4] == 0xa6);
}

static void test_alternative_shifter_mapping(void) {
    fanatec_input_state state = {.accessory = {0, 0, 0, 0, 0x35}};

    fanatec_input_apply_alternative_shifter(&state, true);
    assert(state.accessory[4] == 0xb5);

    fanatec_input_apply_alternative_shifter(&state, false);
    assert(state.accessory[4] == 0x35);
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

static void test_pedal_status(void) {
    fanatec_input_state state = {.status_flags = 0xc1};

    fanatec_input_apply_pedal_status(&state, true, true, true, true, true);
    assert(state.status_flags == 0xff);
    fanatec_input_apply_pedal_status(&state, false, false, true, false, false);
    assert(state.status_flags == 0xc9);
}

static void test_wheel_input_capability_status(void) {
    fanatec_input_state state = {.status_flags = 0x51};

    fanatec_input_apply_wheel_input_capability(&state, true);
    assert(state.status_flags == 0xd1);

    fanatec_input_apply_wheel_input_capability(&state, false);
    assert(state.status_flags == 0x51);
}

static void test_wheel_axis_override_mapping(void) {
    fanatec_input_state state = {
        .pedals = {0x8080, 0x4040, 0x2020},
        .auxiliary_pedal = 0x70,
    };
    WheelAxisOverrides overrides = {
        .axis_5 = {.enabled = true, .value = 0x30},
        .axis_6 = {.enabled = true, .value = 0x50},
        .axis_7 = {.value = 0x10},
        .auxiliary = {.enabled = true, .value = 0x60},
    };

    fanatec_input_apply_wheel_axis_overrides(&state, &overrides);

    assert(state.pedals[0] == 0x3030);
    assert(state.pedals[1] == 0x4040);
    assert(state.pedals[2] == 0x2020);
    assert(state.auxiliary_pedal == 0x60);

    overrides.axis_6.value = 0x20;
    overrides.axis_7.enabled = true;
    overrides.auxiliary.value = 0x80;
    fanatec_input_apply_wheel_axis_overrides(&state, &overrides);

    assert(state.pedals[1] == 0x2020);
    assert(state.pedals[2] == 0x1010);
    assert(state.auxiliary_pedal == 0x60);
}

int main(void) {
    test_encode();
    test_zero_state();
    test_bite_point_update();
    test_validation();
    test_invalid_command_report_mode();
    test_standard_profile_selector_masks_packed_rotary_low_nibble();
    test_official_source_history();
    test_official_first_five_mapping_and_axes();
    test_legacy_mode_maps_auxiliary_high_nibble();
    test_protocol_active_clears_extended_fields();
    test_auxiliary_transfer_code_reaches_native_report_byte();
    test_production_pipeline_first_five_report();
    test_primary_third_button_bank_mapping();
    test_default_auxiliary_button_source();
    test_adapter_auxiliary_merges();
    test_pulse_status_mapping();
    test_compatibility_encode();
    test_wheel_control_mapping();
    test_restricted_wheel_control_mapping();
    test_quaternary_rotary_event_mapping();
    test_wheel_accessory_mapping();
    test_alternative_shifter_mapping();
    test_multi_position_mode_mapping();
    test_multi_position_encoder_mapping();
    test_multi_position_pulse_mapping();
    test_multi_position_constant_mapping();
    test_multi_position_remapped_selector_layout();
    test_unsupported_multi_position_mode_clears_rotaries();
    test_h_pattern_shifter_mapping();
    test_sequential_shifter_mapping();
    test_thermal_effect_limit_status();
    test_pedal_status();
    test_wheel_calibration_status();
    test_wheel_input_capability_status();
    test_wheel_axis_override_mapping();
    return 0;
}
