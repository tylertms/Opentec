#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/vendor_command.h"

static UsbOutputCommand make_output(uint8_t payload[63], uint8_t opcode) {
    payload[0] = opcode;
    return (UsbOutputCommand){
        .kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER,
        .payload = payload,
        .length = 63,
    };
}

static void test_classifies_direct_command_routes(void) {
    static const struct {
        uint8_t opcode;
        UsbVendorCommandKind kind;
    } cases[] = {
        {1, USB_VENDOR_COMMAND_DEVICE_CONTROL_RESPONSE},
        {2, USB_VENDOR_COMMAND_RESPONSE_PREPARATION},
        {3, USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE},
        {4, USB_VENDOR_COMMAND_ACKNOWLEDGEMENT},
        {5, USB_VENDOR_COMMAND_OPERATING_MODE_TRANSITION},
        {8, USB_VENDOR_COMMAND_STATUS_RESPONSE},
        {0x10, USB_VENDOR_COMMAND_EDS_WRITE},
        {0x11, USB_VENDOR_COMMAND_EDS_TRANSFER},
        {0x13, USB_VENDOR_COMMAND_EDS_TRANSFER},
        {0xff, USB_VENDOR_COMMAND_EXTENDED},
    };
    uint8_t payload[63] = {0};

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        UsbOutputCommand output = make_output(payload, cases[index].opcode);
        UsbVendorCommand command;
        assert(usb_vendor_command_decode(&output, &command));
        assert(command.kind == cases[index].kind);
        assert(command.opcode == cases[index].opcode);
        assert(command.arguments == payload + 1);
        assert(command.length == 62);
    }
}

static void test_validates_extended_reset_signature(void) {
    uint8_t payload[63] = {0};
    UsbOutputCommand output = make_output(payload, 0x0a);
    UsbVendorCommand command;

    assert(!usb_vendor_command_decode(&output, &command));
    payload[1] = 1;
    assert(!usb_vendor_command_decode(&output, &command));
    payload[2] = 0x1a;
    assert(usb_vendor_command_decode(&output, &command));
    assert(command.kind == USB_VENDOR_COMMAND_EXTENDED_RESET);
}

static void test_identifies_motor_command_request(void) {
    uint8_t payload[63] = {0xff, 0, 1, 1};
    UsbOutputCommand output = make_output(payload, 0xff);
    UsbVendorCommand command;

    assert(usb_vendor_command_decode(&output, &command));
    assert(usb_vendor_command_requests_motor_command(&command));

    payload[3] = 0;
    assert(!usb_vendor_command_requests_motor_command(&command));
    command.kind = USB_VENDOR_COMMAND_STATUS_RESPONSE;
    payload[3] = 1;
    assert(!usb_vendor_command_requests_motor_command(&command));
    assert(!usb_vendor_command_requests_motor_command(NULL));
}

static void test_rejects_unhandled_payloads(void) {
    uint8_t payload[63] = {0};
    UsbOutputCommand output = make_output(payload, 6);
    UsbVendorCommand command;

    assert(!usb_vendor_command_decode(&output, &command));
    output.length = 62;
    assert(!usb_vendor_command_decode(&output, &command));
    output.length = 63;
    output.kind = USB_OUTPUT_COMMAND_OPERATING_MODE;
    assert(!usb_vendor_command_decode(&output, &command));
    output.kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER;
    output.payload = NULL;
    assert(!usb_vendor_command_decode(&output, &command));
    assert(!usb_vendor_command_decode(NULL, &command));
    assert(!usb_vendor_command_decode(&output, NULL));
}

int main(void) {
    test_classifies_direct_command_routes();
    test_validates_extended_reset_signature();
    test_identifies_motor_command_request();
    test_rejects_unhandled_payloads();
    return 0;
}
