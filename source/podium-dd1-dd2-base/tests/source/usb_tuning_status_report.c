#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/tuning_status_report.h"

static UsbVendorCommand command(uint8_t action, uint8_t value, uint8_t arguments[2]) {
    arguments[0] = action;
    arguments[1] = value;
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_TUNING_STATUS,
        .arguments = arguments,
        .length = 2,
    };
}

static UsbTuningStatusSnapshot populated_snapshot(void) {
    return (UsbTuningStatusSnapshot){
        .base_status = 0x2110,
        .hardware_mode = 0x32,
        .auxiliary_mode = 0x43,
        .auxiliary_flags = 0x54,
        .auxiliary_status = UINT32_C(0x98877665),
        .wheel_status_low = 0xa9,
        .wheel_status_high = 0xfa,
        .wheel_mode = 0xba,
        .button_mode = 0xcb,
        .input = UINT32_C(0x10feeded),
        .adapter_mode = 0x21,
        .adapter = {0x32, 0x43, 0x54, 0x65, 0x76},
        .pedal_status = 0x87,
        .pedal_auxiliary = 0x98,
        .pedal_axis_low = 0xa9,
        .pedal_axis_high = 0xba,
        .tuning_available = true,
        .system_active = true,
        .force_effect = 0x54,
        .system_flags = 0x76,
        .output_status = 0x98,
        .interface_gate = true,
    };
}

static void test_encodes_exact_status_payload(void) {
    UsbTuningStatusReportService service;
    usb_tuning_status_report_service_init(&service);
    uint8_t arguments[2];
    UsbVendorCommand enable = command(1, 0xff, arguments);
    assert(usb_tuning_status_report_apply_command(&service, &enable));

    UsbTuningStatusSnapshot snapshot = populated_snapshot();
    uint8_t report[USB_DEVICE_REPORT_SIZE];
    assert(usb_tuning_status_report_prepare(&service, &snapshot, report));
    static const uint8_t expected[USB_DEVICE_REPORT_SIZE] = {
        0xff, 0x08, 0x10, 0x21, 0x32, 0x09, 0x03, 0x01, 0x01, 0x00, 0x43, 0x54, 0x65,
        0x76, 0x87, 0x98, 0x00, 0x01, 0xa9, 0x3a, 0x00, 0x00, 0x00, 0x00, 0xba, 0xcb,
        0xed, 0xed, 0xfe, 0x10, 0x00, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x00, 0x87,
        0x98, 0xa9, 0xba, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0xd4, 0x00, 0x76, 0x00,
        0x00, 0x0d, 0x00, 0x98, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_publishes_only_when_due(void) {
    UsbTuningStatusReportService service;
    usb_tuning_status_report_service_init(&service);
    UsbTuningStatusSnapshot snapshot = populated_snapshot();
    uint8_t report[USB_DEVICE_REPORT_SIZE];
    assert(!usb_tuning_status_report_prepare(&service, &snapshot, report));

    uint8_t arguments[2];
    UsbVendorCommand enable = command(1, 0xff, arguments);
    assert(usb_tuning_status_report_apply_command(&service, &enable));
    assert(usb_tuning_status_report_prepare(&service, &snapshot, report));
    usb_tuning_status_report_commit(&service, report);
    assert(!usb_tuning_status_report_prepare(&service, &snapshot, report));

    UsbVendorCommand refresh = command(2, 0, arguments);
    assert(usb_tuning_status_report_apply_command(&service, &refresh));
    assert(usb_tuning_status_report_prepare(&service, &snapshot, report));
    usb_tuning_status_report_commit(&service, report);
    assert(!usb_tuning_status_report_prepare(&service, &snapshot, report));

    snapshot.input++;
    assert(usb_tuning_status_report_prepare(&service, &snapshot, report));

    UsbVendorCommand disable = command(1, 0, arguments);
    assert(usb_tuning_status_report_apply_command(&service, &disable));
    assert(!usb_tuning_status_report_prepare(&service, &snapshot, report));
}

static void test_rejects_invalid_requests(void) {
    UsbTuningStatusReportService service;
    usb_tuning_status_report_service_init(&service);
    uint8_t arguments[2];
    UsbVendorCommand request = command(1, 0xff, arguments);
    request.kind = USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT;
    assert(!usb_tuning_status_report_apply_command(&service, &request));
    assert(!usb_tuning_status_report_apply_command(0, &request));
    assert(!usb_tuning_status_report_apply_command(&service, 0));
    assert(!usb_tuning_status_report_prepare(0, 0, 0));
    usb_tuning_status_report_commit(0, 0);
}

int main(void) {
    test_encodes_exact_status_payload();
    test_publishes_only_when_due();
    test_rejects_invalid_requests();
    return 0;
}
