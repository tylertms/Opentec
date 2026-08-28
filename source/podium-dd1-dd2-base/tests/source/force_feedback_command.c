#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/command.h"

static UsbOutputCommand make_command(const uint8_t payload[7]) {
    return (UsbOutputCommand){
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = 7,
    };
}

static void test_decodes_kind_1_magnitudes(void) {
    uint8_t payload[7] = {0x21, 8, 0, 0, 0, 0, 0};
    UsbOutputCommand output = make_command(payload);
    ForceFeedbackCommand command;

    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1);
    assert(command.slot == 2);
    assert(command.magnitude == 65535);

    payload[2] = 0x80;
    assert(force_feedback_command_decode(&output, &command));
    assert(command.magnitude == 0);

    payload[2] = 0xff;
    assert(force_feedback_command_decode(&output, &command));
    assert(command.magnitude == -65535);

    payload[2] = 0;
    payload[3] = 0x80;
    payload[6] = 1;
    assert(force_feedback_command_decode(&output, &command));
    assert(command.magnitude == -1);

    payload[6] = 2;
    assert(force_feedback_command_decode(&output, &command));
    assert(command.magnitude == 0);
}

static void test_decodes_kind_2_configuration(void) {
    const uint8_t payload[7] = {0xa1, 0x0b, 0x12, 0x34, 0xa5, 0x11, 0x80};
    UsbOutputCommand output = make_command(payload);
    ForceFeedbackCommand command;

    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2);
    assert(command.slot == 10);
    assert(command.positions[0] == 0x12);
    assert(command.positions[1] == 0x34);
    assert(command.axis_modes[0] == 5);
    assert(command.axis_modes[1] == 10);
    assert(command.directions[0] == 1);
    assert(command.directions[1] == 1);
    assert(command.strength == 0x8080);
}

static void test_decodes_kind_3_configuration(void) {
    const uint8_t payload[7] = {0xf1, 0x0c, 0xa7, 1, 0xb9, 0, 0x40};
    UsbOutputCommand output = make_command(payload);
    ForceFeedbackCommand command;

    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3);
    assert(command.slot == 15);
    assert(command.mode == 7);
    assert(command.directions[0] == -1);
    assert(command.axis_modes[0] == 9);
    assert(command.directions[1] == 1);
    assert(command.strength == 0x4040);
}

static void test_decodes_effect_control(void) {
    uint8_t payload[7] = {0x73, 0, 0, 0, 0, 0, 0};
    UsbOutputCommand output = make_command(payload);
    ForceFeedbackCommand command;

    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_CLEAR_EFFECT);
    assert(command.slot == 7);

    payload[0] = 0x14;
    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_ACTIVATE_POSITION_EFFECT);
    assert(command.slot == FORCE_FEEDBACK_POSITION_EFFECT_SLOT);

    payload[0] = 0x15;
    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_CLEAR_POSITION_EFFECT);
    assert(command.slot == FORCE_FEEDBACK_POSITION_EFFECT_SLOT);
}

static void test_decodes_output_gates(void) {
    uint8_t payload[7] = {0xfa, 0xc7, 0, 0, 0, 0, 0};
    UsbOutputCommand output = make_command(payload);
    ForceFeedbackCommand command;

    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT);
    assert(command.output_disabled);

    payload[1] = 0;
    assert(force_feedback_command_decode(&output, &command));
    assert(!command.output_disabled);

    payload[0] = 0xfb;
    payload[1] = 1;
    assert(force_feedback_command_decode(&output, &command));
    assert(command.kind == FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT);
    assert(command.output_disabled);
}

static void test_rejects_other_short_commands(void) {
    uint8_t payload[7] = {0x11, 7, 0, 0, 0, 0, 0};
    UsbOutputCommand output = make_command(payload);
    ForceFeedbackCommand command;

    assert(!force_feedback_command_decode(&output, &command));

    payload[0] = 4;
    assert(!force_feedback_command_decode(&output, &command));

    payload[0] = 5;
    assert(!force_feedback_command_decode(&output, &command));

    payload[0] = 0xf8;
    assert(!force_feedback_command_decode(&output, &command));

    output.length = 6;
    assert(!force_feedback_command_decode(&output, &command));
    output.length = 7;
    output.kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER;
    assert(!force_feedback_command_decode(&output, &command));
    assert(!force_feedback_command_decode(NULL, &command));
    assert(!force_feedback_command_decode(&output, NULL));
}

int main(void) {
    test_decodes_kind_1_magnitudes();
    test_decodes_kind_2_configuration();
    test_decodes_kind_3_configuration();
    test_decodes_effect_control();
    test_decodes_output_gates();
    test_rejects_other_short_commands();
    return 0;
}
