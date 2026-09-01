#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "pedal/calibration_command.h"

static UsbOperatingModeCommand source_command(uint8_t selector, uint8_t first, uint8_t second,
                                              uint8_t third) {
    return (UsbOperatingModeCommand){
        .opcode = 1,
        .parameters = {selector, first, second, third},
    };
}

static void test_decodes_supported_selectors(void) {
    static const PedalCalibrationCommandKind expected[] = {
        PEDAL_CALIBRATION_COMMAND_UP,     PEDAL_CALIBRATION_COMMAND_DOWN,
        PEDAL_CALIBRATION_COMMAND_ENABLE, PEDAL_CALIBRATION_COMMAND_DISABLE,
        PEDAL_CALIBRATION_COMMAND_INPUT,
    };
    for (uint8_t index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
        UsbOperatingModeCommand source = source_command((uint8_t)(0x11 + index), 1, 2, 3);
        PedalCalibrationCommand command;
        assert(pedal_calibration_command_decode(&source, &command));
        assert(command.kind == expected[index]);
        assert(command.input[0] == 1);
        assert(command.input[1] == 2);
        assert(command.input[2] == 3);
    }
}

static void test_rejects_other_operating_mode_commands(void) {
    PedalCalibrationCommand command;
    UsbOperatingModeCommand source = source_command(0x10, 0, 0, 0);
    assert(!pedal_calibration_command_decode(&source, &command));
    source.parameters[0] = 0x16;
    assert(!pedal_calibration_command_decode(&source, &command));
    source.opcode = 2;
    assert(!pedal_calibration_command_decode(&source, &command));
    assert(!pedal_calibration_command_decode(NULL, &command));
    assert(!pedal_calibration_command_decode(&source, NULL));
}

static void test_routes_control_and_auxiliary_actions_independently(void) {
    PedalCalibrationCommand command;
    UsbOperatingModeCommand source = source_command(0x11, 0, 0, 0);
    assert(pedal_calibration_command_decode(&source, &command));

    PedalCalibrationActions actions = pedal_calibration_command_route(&command, true, true);
    assert(actions.pedal_control == PEDAL_V3_CONTROL_UP);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_MAXIMUM);

    actions = pedal_calibration_command_route(&command, false, true);
    assert(actions.pedal_control == 0);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_MAXIMUM);

    source.parameters[0] = 0x12;
    assert(pedal_calibration_command_decode(&source, &command));
    actions = pedal_calibration_command_route(&command, true, false);
    assert(actions.pedal_control == PEDAL_V3_CONTROL_DOWN);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_NONE);
}

static void test_routes_enable_and_disable_to_auxiliary_reset(void) {
    PedalCalibrationCommand command;
    UsbOperatingModeCommand source = source_command(0x13, 0, 0, 0);
    assert(pedal_calibration_command_decode(&source, &command));
    PedalCalibrationActions actions = pedal_calibration_command_route(&command, true, true);
    assert(actions.pedal_control == PEDAL_V3_CONTROL_ENABLE);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_RESET);

    source.parameters[0] = 0x14;
    assert(pedal_calibration_command_decode(&source, &command));
    actions = pedal_calibration_command_route(&command, true, true);
    assert(actions.pedal_control == PEDAL_V3_CONTROL_DISABLE);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_RESET);
}

static void test_routes_the_reserved_auxiliary_input_payload(void) {
    PedalCalibrationCommand command;
    UsbOperatingModeCommand source = source_command(0x15, 1, 3, 0);
    assert(pedal_calibration_command_decode(&source, &command));
    PedalCalibrationActions actions = pedal_calibration_command_route(&command, true, true);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_MINIMUM);
    assert(!actions.pedal_input_pending);

    command.input[2] = 7;
    actions = pedal_calibration_command_route(&command, true, true);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_NONE);
    assert(!actions.pedal_input_pending);
}

static void test_forwards_other_input_payloads_only_during_pedal_calibration(void) {
    PedalCalibrationCommand command;
    UsbOperatingModeCommand source = source_command(0x15, 4, 5, 6);
    assert(pedal_calibration_command_decode(&source, &command));

    PedalCalibrationActions actions = pedal_calibration_command_route(&command, true, true);
    assert(actions.auxiliary_action == PEDAL_AUXILIARY_CALIBRATION_NONE);
    assert(actions.pedal_input_pending);
    assert(actions.pedal_input[0] == 4);
    assert(actions.pedal_input[1] == 5);
    assert(actions.pedal_input[2] == 6);

    actions = pedal_calibration_command_route(&command, false, true);
    assert(!actions.pedal_input_pending);
    actions = pedal_calibration_command_route(NULL, true, true);
    assert(!actions.pedal_input_pending);
}

int main(void) {
    test_decodes_supported_selectors();
    test_rejects_other_operating_mode_commands();
    test_routes_control_and_auxiliary_actions_independently();
    test_routes_enable_and_disable_to_auxiliary_reset();
    test_routes_the_reserved_auxiliary_input_payload();
    test_forwards_other_input_payloads_only_during_pedal_calibration();
    return 0;
}
