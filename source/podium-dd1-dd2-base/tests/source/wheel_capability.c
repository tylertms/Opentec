#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/capability.h"

static void test_caches_and_maps_report_capabilities(void) {
    WheelCapabilityState state = {.report_flags = 0xffe1};

    wheel_capability_update(&state, 6, 0x34, 0x3f);

    assert(state.capability_flags == 0x3f34);
    assert(state.report_flags == 0xffff);
    assert(state.calibration_available);
    assert(state.tuning_menu_available);

    wheel_capability_update(&state, 6, 0x12, 0);
    assert(state.capability_flags == 0x0012);
    assert(state.report_flags == 0xffe1);
    assert(!state.calibration_available);
    assert(!state.tuning_menu_available);
}

static void test_applies_calibration_mode_defaults(void) {
    static const uint8_t forced_available[] = {5, 7, 8, 0x10, 0x12};
    static const uint8_t forced_unavailable[] = {9, 0x0b, 0x11, 0x15, 0x16, 0x1d};
    WheelCapabilityState state = {0};

    for (uint8_t index = 0; index < sizeof(forced_available); index++) {
        wheel_capability_update(&state, forced_available[index], 0, 0);
        assert(state.calibration_available);
    }
    for (uint8_t index = 0; index < sizeof(forced_unavailable); index++) {
        wheel_capability_update(&state, forced_unavailable[index], 0, UINT8_MAX);
        assert(!state.calibration_available);
    }

    wheel_capability_update(&state, 6, 0, 1);
    assert(state.calibration_available);
    wheel_capability_update(&state, 6, 0, 0);
    assert(!state.calibration_available);
}

static void test_gates_latched_input_capability_by_wheel_mode(void) {
    static const uint8_t supported_modes[] = {4, 6, 12, 14, 15, 19, 20, 21, 22, 23, 28};
    WheelCapabilityState state = {.input_available = true};

    for (uint8_t mode = 0; mode <= 30; mode++) {
        bool expected = false;
        for (uint8_t index = 0; index < sizeof(supported_modes); index++) {
            expected |= mode == supported_modes[index];
        }
        assert(wheel_capability_input_available(&state, mode) == expected);
    }

    state.input_available = false;
    assert(!wheel_capability_input_available(&state, 4));
    assert(!wheel_capability_input_available(NULL, 4));
}

static void test_resolves_tuning_menu_availability(void) {
    static const uint8_t inherent_modes[] = {6, 7, 9, 11, 18, 29};
    static const uint8_t reported_modes[] = {10, 19, 20, 21};
    WheelCapabilityState state = {0};

    for (uint8_t mode = 0; mode <= 30; mode++) {
        bool expected = false;
        for (uint8_t index = 0; index < sizeof(inherent_modes); index++) {
            expected |= mode == inherent_modes[index];
        }
        assert(wheel_capability_tuning_menu_available(&state, mode) == expected);
    }
    state.tuning_menu_available = true;
    for (uint8_t index = 0; index < sizeof(reported_modes); index++) {
        assert(wheel_capability_tuning_menu_available(&state, reported_modes[index]));
    }
    assert(!wheel_capability_tuning_menu_available(NULL, 10));
}

static UsbOperatingModeCommand multi_position_command(uint8_t selector, uint8_t mode) {
    UsbOperatingModeCommand command = {.opcode = 1};
    command.parameters[0] = selector;
    command.parameters[1] = mode;
    return command;
}

static void test_applies_multi_position_override_commands(void) {
    WheelCapabilityState state;
    wheel_capability_init(&state);
    assert(state.multi_position_override == UINT8_MAX);

    UsbOperatingModeCommand command = multi_position_command(0x16, 2);
    assert(wheel_capability_apply_multi_position_command(&state, &command));
    assert(state.multi_position_override == 2);

    command.parameters[1] = TUNING_MULTI_POSITION_AUTOMATIC;
    assert(wheel_capability_apply_multi_position_command(&state, &command));
    assert(state.multi_position_override == UINT8_MAX);
}

static void test_rejects_other_multi_position_commands(void) {
    WheelCapabilityState state;
    wheel_capability_init(&state);
    UsbOperatingModeCommand command = multi_position_command(0x15, 2);
    assert(!wheel_capability_apply_multi_position_command(&state, &command));
    command.parameters[0] = 0x16;
    command.opcode = 0;
    assert(!wheel_capability_apply_multi_position_command(&state, &command));
    assert(!wheel_capability_apply_multi_position_command(NULL, &command));
    assert(!wheel_capability_apply_multi_position_command(&state, NULL));
}

static void test_resolves_multi_position_mode(void) {
    WheelCapabilityState state;
    wheel_capability_init(&state);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_ENCODER, 9, true) ==
           TUNING_MULTI_POSITION_ENCODER);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_PULSE, 9, true) ==
           TUNING_MULTI_POSITION_PULSE);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_CONSTANT, 9, true) ==
           TUNING_MULTI_POSITION_CONSTANT);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_AUTOMATIC, 9, true) ==
           TUNING_MULTI_POSITION_PULSE);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_AUTOMATIC, 8, true) ==
           TUNING_MULTI_POSITION_ENCODER);

    state.multi_position_override = TUNING_MULTI_POSITION_CONSTANT;
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_AUTOMATIC, 9, true) ==
           TUNING_MULTI_POSITION_CONSTANT);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_CONSTANT, 4, false) ==
           TUNING_MULTI_POSITION_ENCODER);
    assert(wheel_capability_multi_position_mode(NULL, TUNING_MULTI_POSITION_CONSTANT, 9, true) ==
           TUNING_MULTI_POSITION_ENCODER);
    assert(wheel_capability_multi_position_mode(&state, (TuningMultiPositionMode)4, 9, true) ==
           TUNING_MULTI_POSITION_ENCODER);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_CONSTANT, 4, true) ==
           TUNING_MULTI_POSITION_CONSTANT);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_CONSTANT, 6, false) ==
           TUNING_MULTI_POSITION_ENCODER);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_CONSTANT, 10,
                                                false) == TUNING_MULTI_POSITION_CONSTANT);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_CONSTANT, 29,
                                                false) == TUNING_MULTI_POSITION_CONSTANT);
    assert(wheel_capability_multi_position_mode(&state, TUNING_MULTI_POSITION_CONSTANT, 30, true) ==
           TUNING_MULTI_POSITION_ENCODER);
}

static void test_reports_multi_position_support(void) {
    assert(wheel_capability_multi_position_supported(4, true));
    assert(!wheel_capability_multi_position_supported(4, false));
    assert(wheel_capability_multi_position_supported(9, false));
    assert(wheel_capability_multi_position_supported(23, false));
    assert(wheel_capability_multi_position_supported(29, false));
    assert(!wheel_capability_multi_position_supported(22, true));
    assert(!wheel_capability_multi_position_supported(30, true));
}

int main(void) {
    test_caches_and_maps_report_capabilities();
    test_applies_calibration_mode_defaults();
    test_gates_latched_input_capability_by_wheel_mode();
    test_resolves_tuning_menu_availability();
    test_applies_multi_position_override_commands();
    test_rejects_other_multi_position_commands();
    test_resolves_multi_position_mode();
    test_reports_multi_position_support();
    return 0;
}
