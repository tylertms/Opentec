#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/output_command.h"

static UsbDeviceOutputReport make_report(uint8_t report_id, uint8_t length) {
    UsbDeviceOutputReport report = {
        .report_type = USB_DEVICE_HID_REPORT_OUTPUT,
        .report_id = report_id,
        .length = length,
    };
    report.data[0] = report_id;
    for (uint8_t index = 1; index < length; index++) {
        report.data[index] = index;
    }
    return report;
}

static void test_decodes_operating_mode_report(void) {
    UsbDeviceOutputReport report = make_report(1, 8);
    UsbOutputCommand command;

    assert(usb_output_command_decode(&report, &command));
    assert(command.kind == USB_OUTPUT_COMMAND_OPERATING_MODE);
    assert(command.payload == report.data + 1);
    assert(command.length == 7);
    assert(command.payload[0] == 1);
    assert(command.payload[6] == 7);
}

static void test_decodes_vendor_transfer_report(void) {
    UsbDeviceOutputReport report = make_report(0xff, 64);
    UsbOutputCommand command;

    assert(usb_output_command_decode(&report, &command));
    assert(command.kind == USB_OUTPUT_COMMAND_VENDOR_TRANSFER);
    assert(command.payload == report.data + 1);
    assert(command.length == 63);
    assert(command.payload[0] == 1);
    assert(command.payload[62] == 63);
}

static void test_rejects_unhandled_reports(void) {
    UsbOutputCommand command;
    UsbDeviceOutputReport report = make_report(2, 8);
    assert(!usb_output_command_decode(&report, &command));

    report = make_report(1, 7);
    assert(!usb_output_command_decode(&report, &command));

    report = make_report(0xff, 63);
    assert(!usb_output_command_decode(&report, &command));

    report = make_report(1, 8);
    report.data[0] = 2;
    assert(!usb_output_command_decode(&report, &command));

    report = make_report(1, 8);
    report.report_type = USB_DEVICE_HID_REPORT_FEATURE;
    assert(!usb_output_command_decode(&report, &command));

    assert(!usb_output_command_decode(NULL, &command));
    assert(!usb_output_command_decode(&report, NULL));
}

int main(void) {
    test_decodes_operating_mode_report();
    test_decodes_vendor_transfer_report();
    test_rejects_unhandled_reports();
    return 0;
}
