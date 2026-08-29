#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

static void test_decodes_command_envelope(void) {
    const uint8_t payload[] = {0xf8, 9, 5, 0x11, 0x22, 0x33, 0x44};
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = sizeof(payload),
    };
    UsbOperatingModeCommand command;

    assert(usb_operating_mode_command_decode(&output, &command));
    assert(command.opcode == 5);
    assert(command.parameters[0] == 0x11);
    assert(command.parameters[1] == 0x22);
    assert(command.parameters[2] == 0x33);
    assert(command.parameters[3] == 0x44);
}

static void test_rejects_other_envelopes(void) {
    uint8_t payload[] = {0xf8, 9, 5, 0, 0, 0, 0};
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = sizeof(payload),
    };
    UsbOperatingModeCommand command;

    payload[0] = 0xf7;
    assert(!usb_operating_mode_command_decode(&output, &command));
    payload[0] = 0xf8;
    payload[1] = 8;
    assert(!usb_operating_mode_command_decode(&output, &command));
    payload[1] = 9;
    output.length = 6;
    assert(!usb_operating_mode_command_decode(&output, &command));
    output.length = 7;
    output.kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER;
    assert(!usb_operating_mode_command_decode(&output, &command));
    output.kind = USB_OUTPUT_COMMAND_SHORT;
    output.payload = NULL;
    assert(!usb_operating_mode_command_decode(&output, &command));
    assert(!usb_operating_mode_command_decode(NULL, &command));
    assert(!usb_operating_mode_command_decode(&output, NULL));
}

static void test_identifies_native_reset(void) {
    UsbOperatingModeCommand command = {
        .opcode = 1,
        .parameters = {1, 0, 0, 0},
    };

    assert(usb_operating_mode_command_requests_native_reset(&command));
    command.parameters[0] = 0;
    assert(!usb_operating_mode_command_requests_native_reset(&command));
    command.parameters[0] = 1;
    command.opcode = 2;
    assert(!usb_operating_mode_command_requests_native_reset(&command));
    assert(!usb_operating_mode_command_requests_native_reset(NULL));
}

static void test_decodes_operating_status(void) {
    UsbOperatingModeCommand command = {.opcode = 2, .parameters = {0}};
    bool enabled = true;

    assert(usb_operating_mode_command_decode_status(&command, &enabled));
    assert(!enabled);

    command.parameters[0] = 0xa5;
    assert(usb_operating_mode_command_decode_status(&command, &enabled));
    assert(enabled);

    command.opcode = 3;
    assert(!usb_operating_mode_command_decode_status(&command, &enabled));
    assert(!usb_operating_mode_command_decode_status(NULL, &enabled));
    assert(!usb_operating_mode_command_decode_status(&command, NULL));
}

int main(void) {
    test_decodes_command_envelope();
    test_rejects_other_envelopes();
    test_identifies_native_reset();
    test_decodes_operating_status();
    return 0;
}
