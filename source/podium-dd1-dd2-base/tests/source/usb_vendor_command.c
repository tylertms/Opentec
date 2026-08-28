#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
        {4, USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT},
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

static void test_decodes_wheel_transfer_commands(void) {
    uint8_t payload[63] = {0xff, 0xe0, 0x02, 0x05, 1};
    UsbOutputCommand output = make_output(payload, 0xff);
    UsbVendorCommand command;
    UsbWheelTransferCommand transfer;

    assert(usb_vendor_command_decode(&output, &command));
    assert(usb_vendor_command_decode_wheel_transfer(&command, &transfer));
    assert(transfer.request == WHEEL_TRANSFER_READ);
    assert(transfer.action == USB_WHEEL_TRANSFER_START);

    payload[3] = 4;
    payload[4] = 2;
    assert(usb_vendor_command_decode_wheel_transfer(&command, &transfer));
    assert(transfer.request == WHEEL_TRANSFER_WRITE);
    assert(transfer.action == USB_WHEEL_TRANSFER_STATUS);

    payload[2] = 3;
    assert(!usb_vendor_command_decode_wheel_transfer(&command, &transfer));
    payload[2] = 2;
    payload[4] = 3;
    assert(!usb_vendor_command_decode_wheel_transfer(&command, &transfer));
    assert(!usb_vendor_command_decode_wheel_transfer(NULL, &transfer));
    assert(!usb_vendor_command_decode_wheel_transfer(&command, NULL));
}

static void test_encodes_wheel_transfer_status(void) {
    uint8_t report[USB_DEVICE_REPORT_SIZE];

    usb_vendor_command_encode_wheel_transfer_response(WHEEL_TRANSFER_READ, WHEEL_TRANSFER_COMPLETE,
                                                      report);
    static const uint8_t expected_read[] = {0xff, 0xe0, 0x02, 0x05, 2};
    assert(memcmp(report, expected_read, sizeof(expected_read)) == 0);
    for (size_t index = sizeof(expected_read); index < sizeof(report); index++) {
        assert(report[index] == 0);
    }

    usb_vendor_command_encode_wheel_transfer_response(WHEEL_TRANSFER_WRITE,
                                                      WHEEL_TRANSFER_INVALID_RESPONSE, report);
    static const uint8_t expected_write[] = {0xff, 0xe0, 0x02, 0x04, 0xfd};
    assert(memcmp(report, expected_write, sizeof(expected_write)) == 0);
}

static void test_rejects_unhandled_payloads(void) {
    uint8_t payload[63] = {0};
    UsbOutputCommand output = make_output(payload, 6);
    UsbVendorCommand command;

    assert(!usb_vendor_command_decode(&output, &command));
    output.length = 62;
    assert(!usb_vendor_command_decode(&output, &command));
    output.length = 63;
    output.kind = USB_OUTPUT_COMMAND_SHORT;
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
    test_decodes_wheel_transfer_commands();
    test_encodes_wheel_transfer_status();
    test_rejects_unhandled_payloads();
    return 0;
}
