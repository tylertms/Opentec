#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

static void test_decodes_command_envelope(void) {
    const uint8_t payload[] = {0xf8, 9, 5, 0x11, 0x22, 0x33, 0x44};
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_OPERATING_MODE,
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
        .kind = USB_OUTPUT_COMMAND_OPERATING_MODE,
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
    output.kind = USB_OUTPUT_COMMAND_OPERATING_MODE;
    output.payload = NULL;
    assert(!usb_operating_mode_command_decode(&output, &command));
    assert(!usb_operating_mode_command_decode(NULL, &command));
    assert(!usb_operating_mode_command_decode(&output, NULL));
}

int main(void) {
    test_decodes_command_envelope();
    test_rejects_other_envelopes();
    return 0;
}
