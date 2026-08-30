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
        {1, USB_VENDOR_COMMAND_WHEEL_OUTPUT_REPORT},   {2, USB_VENDOR_COMMAND_TUNING_MENU},
        {3, USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE}, {4, USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT},
        {5, USB_VENDOR_COMMAND_REMOTE_TUNING},         {8, USB_VENDOR_COMMAND_STATUS_RESPONSE},
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

static void test_classifies_xbox_tunnel_payload(void) {
    uint8_t payload[59] = {5, 2, 1};
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER,
        .payload = payload,
        .length = sizeof(payload),
    };
    UsbVendorCommand command;

    assert(usb_vendor_command_decode(&output, &command));
    assert(command.kind == USB_VENDOR_COMMAND_REMOTE_TUNING);
    assert(command.arguments == payload + 1 && command.length == 58);
}

static void test_rejects_unsupported_script_groups(void) {
    uint8_t payload[63] = {0};
    UsbOutputCommand output = make_output(payload, 0x0a);
    UsbVendorCommand command;

    assert(!usb_vendor_command_decode(&output, &command));
    payload[1] = 1;
    assert(!usb_vendor_command_decode(&output, &command));
    payload[2] = 0x1a;
    assert(!usb_vendor_command_decode(&output, &command));
}

static void test_classifies_script_status_query(void) {
    uint8_t payload[63] = {[4] = 6};
    UsbOutputCommand output = make_output(payload, 0x0a);
    UsbVendorCommand command;

    assert(usb_vendor_command_decode(&output, &command));
    assert(command.kind == USB_VENDOR_COMMAND_SCRIPT_STATUS);

    payload[4] = 5;
    payload[5] = 15;
    assert(!usb_vendor_command_decode(&output, &command));
    payload[5] = 0;
    payload[4] = 7;
    assert(usb_vendor_command_decode(&output, &command));
    assert(command.kind == USB_VENDOR_COMMAND_SCRIPT_VALUES);
    payload[4] = 8;
    assert(usb_vendor_command_decode(&output, &command));
    assert(command.kind == USB_VENDOR_COMMAND_SCRIPT_AXES);
    payload[1] = 2;
    assert(!usb_vendor_command_decode(&output, &command));
}

static void test_decodes_script_sample_query(void) {
    uint8_t payload[63] = {[4] = 4, [5] = 0xf5, [6] = 1};
    UsbOutputCommand output = make_output(payload, 0x0a);
    UsbVendorCommand command;
    uint16_t index = 0;

    assert(usb_vendor_command_decode(&output, &command));
    assert(command.kind == USB_VENDOR_COMMAND_SCRIPT_SAMPLES);
    assert(usb_vendor_command_script_sample_index(&command, &index));
    assert(index == 501);

    payload[5] = 0xf6;
    assert(!usb_vendor_command_decode(&output, &command));
    assert(!usb_vendor_command_script_sample_index(&command, &index));
    assert(!usb_vendor_command_script_sample_index(NULL, &index));
    assert(!usb_vendor_command_script_sample_index(&command, NULL));
    command.kind = USB_VENDOR_COMMAND_SCRIPT_STATUS;
    assert(!usb_vendor_command_script_sample_index(&command, &index));
}

static void test_decodes_script_slot_query(void) {
    uint8_t payload[63] = {[4] = 5, [5] = 14};
    UsbOutputCommand output = make_output(payload, 0x0a);
    UsbVendorCommand command;
    uint8_t index = 0;

    assert(usb_vendor_command_decode(&output, &command));
    assert(command.kind == USB_VENDOR_COMMAND_SCRIPT_SLOT);
    assert(usb_vendor_command_script_slot_index(&command, &index));
    assert(index == 14);

    payload[5] = 15;
    assert(!usb_vendor_command_decode(&output, &command));
    assert(!usb_vendor_command_script_slot_index(&command, &index));
    assert(!usb_vendor_command_script_slot_index(NULL, &index));
    assert(!usb_vendor_command_script_slot_index(&command, NULL));
    command.kind = USB_VENDOR_COMMAND_SCRIPT_STATUS;
    assert(!usb_vendor_command_script_slot_index(&command, &index));
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

static void test_decodes_tuning_menu_wheel_report(void) {
    uint8_t payload[63] = {2, 1};
    UsbOutputCommand output = make_output(payload, 2);
    UsbVendorCommand command;

    assert(usb_vendor_command_decode(&output, &command));
    assert(usb_vendor_command_decode_wheel_report_seventeen(&command) == payload + 2);

    payload[1] = 2;
    assert(usb_vendor_command_decode_wheel_report_seventeen(&command) == NULL);
    payload[1] = 1;
    command.length--;
    assert(usb_vendor_command_decode_wheel_report_seventeen(&command) == NULL);
    assert(usb_vendor_command_decode_wheel_report_seventeen(NULL) == NULL);
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
    output.length = 64;
    assert(!usb_vendor_command_decode(&output, &command));
    output.length = 0;
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
    test_classifies_xbox_tunnel_payload();
    test_rejects_unsupported_script_groups();
    test_classifies_script_status_query();
    test_decodes_script_sample_query();
    test_decodes_script_slot_query();
    test_identifies_motor_command_request();
    test_decodes_wheel_transfer_commands();
    test_decodes_tuning_menu_wheel_report();
    test_encodes_wheel_transfer_status();
    test_rejects_unhandled_payloads();
    return 0;
}
