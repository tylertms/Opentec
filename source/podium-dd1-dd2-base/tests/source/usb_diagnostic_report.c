#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/diagnostic_report.h"
#include "usb/vendor_command.h"

static UsbVendorCommand command(uint8_t action, uint8_t value, uint8_t arguments[2]) {
    arguments[0] = action;
    arguments[1] = value;
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT,
        .arguments = arguments,
        .length = 2,
    };
}

static UsbDiagnosticSnapshot populated_snapshot(void) {
    return (UsbDiagnosticSnapshot){
        .base_mode = 0x12,
        .base_temperatures_c = {0x2313, 0x4524},
        .system_seconds = UINT32_C(0x56453423),
        .transport_error_count = UINT32_C(0x78675645),
        .motor =
            {
                .version = 0x89,
                .initial_status = -102,
                .motor_temperature = 0xab9a,
                .driver_temperature = 0xcdac,
                .reserved = {0xde, 0xef},
                .runtime_seconds = UINT32_C(0x213201f0),
                .motor_torque = 0x4354,
            },
        .wheel_status =
            {
                .status_high = 0x65,
                .status_low = 0x76,
                .accessory_value = 0x9887,
                .runtime_seconds = UINT32_C(0xbaab9a89),
                .runtime_counter = UINT32_C(0xdccdbcab),
                .trailing_status = 0xed,
            },
        .cooling =
            {
                .phase = 0xfe,
                .output_duty_percent = 0x0f,
                .primary_delay_seconds = 0x10,
                .secondary_delay_seconds = 0x21,
                .low_threshold_offset = 0x32,
                .high_threshold_offset = 0x43,
            },
        .pwm = {0x54, 0x65},
        .pulse = {0x8776, 0xa998},
        .auxiliary_position = {0xba, 0xdccb},
        .wheel_position = (int32_t)UINT32_C(0x10feeded),
        .wheel_velocity = (int32_t)UINT32_C(0x54433221),
    };
}

static void test_encodes_exact_diagnostic_payload(void) {
    UsbDiagnosticReportService service;
    usb_diagnostic_report_service_init(&service);
    uint8_t arguments[2];
    UsbVendorCommand enable = command(1, 0xff, arguments);
    assert(usb_diagnostic_report_apply_command(&service, &enable));

    UsbDiagnosticSnapshot snapshot = populated_snapshot();
    uint8_t report[USB_DEVICE_REPORT_SIZE];
    assert(usb_diagnostic_report_prepare(&service, &snapshot, report));
    static const uint8_t expected[USB_DEVICE_REPORT_SIZE] = {
        0xff, 0x04, 0x03, 0x09, 0x12, 0x13, 0x23, 0x24, 0x45, 0x23, 0x34, 0x45, 0x56,
        0x45, 0x56, 0x67, 0x78, 0x89, 0x9a, 0x9a, 0xab, 0xac, 0xcd, 0xde, 0xef, 0xf0,
        0x01, 0x32, 0x21, 0x54, 0x43, 0x65, 0x76, 0x87, 0x98, 0x89, 0x9a, 0xab, 0xba,
        0xab, 0xbc, 0xcd, 0xdc, 0xed, 0xfe, 0x0f, 0x10, 0x21, 0x32, 0x43, 0x54, 0x65,
        0x76, 0x87, 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xed, 0xfe, 0x10, 0x21,
    };
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_publishes_only_when_due(void) {
    UsbDiagnosticReportService service;
    usb_diagnostic_report_service_init(&service);
    UsbDiagnosticSnapshot snapshot = populated_snapshot();
    uint8_t report[USB_DEVICE_REPORT_SIZE];
    assert(!usb_diagnostic_report_prepare(&service, &snapshot, report));

    uint8_t arguments[2];
    UsbVendorCommand enable = command(1, 0xff, arguments);
    assert(usb_diagnostic_report_apply_command(&service, &enable));
    assert(usb_diagnostic_report_prepare(&service, &snapshot, report));
    usb_diagnostic_report_commit(&service, report);
    assert(!usb_diagnostic_report_prepare(&service, &snapshot, report));

    UsbVendorCommand refresh = command(2, 0, arguments);
    assert(usb_diagnostic_report_apply_command(&service, &refresh));
    assert(usb_diagnostic_report_prepare(&service, &snapshot, report));
    usb_diagnostic_report_commit(&service, report);
    assert(!usb_diagnostic_report_prepare(&service, &snapshot, report));

    snapshot.system_seconds++;
    assert(usb_diagnostic_report_prepare(&service, &snapshot, report));

    UsbVendorCommand disable = command(1, 0, arguments);
    assert(usb_diagnostic_report_apply_command(&service, &disable));
    assert(!usb_diagnostic_report_prepare(&service, &snapshot, report));
}

static void test_rejects_other_routes(void) {
    UsbDiagnosticReportService service;
    usb_diagnostic_report_service_init(&service);
    uint8_t arguments[2];
    UsbVendorCommand request = command(1, 0xff, arguments);
    request.kind = USB_VENDOR_COMMAND_TUNING_STATUS;
    assert(!usb_diagnostic_report_apply_command(&service, &request));
    assert(!usb_diagnostic_report_apply_command(NULL, &request));
    assert(!usb_diagnostic_report_apply_command(&service, NULL));
}

int main(void) {
    test_encodes_exact_diagnostic_payload();
    test_publishes_only_when_due();
    test_rejects_other_routes();
    return 0;
}
