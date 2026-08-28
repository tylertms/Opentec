#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "pedal/protocol_command.h"

static void test_decodes_a_protocol_tuple_update(void) {
    UsbOperatingModeCommand source = {
        .opcode = 1,
        .parameters = {4, 0x12, 0x34, 0x56},
    };
    PedalProtocolCommand command;

    assert(pedal_protocol_command_decode(&source, &command));
    assert(command.kind == PEDAL_PROTOCOL_COMMAND_UPDATE);
    assert(command.value == 0x12);
    assert(command.first == 0x34);
    assert(command.second == 0x56);
}

static void test_decodes_a_legacy_scale_update(void) {
    UsbOperatingModeCommand source = {
        .opcode = 1,
        .parameters = {8, 0x78, 0, 0},
    };
    PedalProtocolCommand command;

    assert(pedal_protocol_command_decode(&source, &command));
    assert(command.kind == PEDAL_PROTOCOL_COMMAND_LEGACY_SCALE);
    assert(command.value == 0x78);
}

static void test_rejects_other_device_controls(void) {
    UsbOperatingModeCommand source = {
        .opcode = 1,
        .parameters = {5, 0, 0, 0},
    };
    PedalProtocolCommand command;

    assert(!pedal_protocol_command_decode(&source, &command));
    source.opcode = 2;
    source.parameters[0] = 4;
    assert(!pedal_protocol_command_decode(&source, &command));
    assert(!pedal_protocol_command_decode(NULL, &command));
    assert(!pedal_protocol_command_decode(&source, NULL));
}

int main(void) {
    test_decodes_a_protocol_tuple_update();
    test_decodes_a_legacy_scale_update();
    test_rejects_other_device_controls();
    return 0;
}
