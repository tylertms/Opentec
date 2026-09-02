#include "usb/playstation_wheel_value.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static UsbDeviceOutputReport make_report(uint8_t flags, uint8_t low, uint8_t high) {
    UsbDeviceOutputReport report = {
        .report_type = USB_DEVICE_HID_REPORT_OUTPUT,
        .report_id = 5,
        .length = USB_DEVICE_REPORT_SIZE,
        .data = {5, flags},
    };
    report.data[4] = low;
    report.data[5] = high;
    return report;
}

static void test_applies_asserted_value_in_wheel_order(void) {
    UsbPlaystationWheelValue value;
    usb_playstation_wheel_value_init(&value);
    UsbDeviceOutputReport report = make_report(1, 0x34, 0x12);

    assert(usb_playstation_wheel_value_apply(&value, &report, 500));
    const uint8_t *axes = usb_playstation_wheel_value_axes(&value);
    assert(axes[0] == 0x12);
    assert(axes[1] == 0x34);
    assert(value.deadline_ms == 3500);
    assert(value.release_pending);
}

static void test_applies_one_release_value(void) {
    UsbPlaystationWheelValue value;
    usb_playstation_wheel_value_init(&value);
    UsbDeviceOutputReport report = make_report(1, 0x34, 0x12);
    assert(usb_playstation_wheel_value_apply(&value, &report, 500));

    report = make_report(0, 0x78, 0x56);
    assert(usb_playstation_wheel_value_apply(&value, &report, 600));
    assert(value.legacy_axes[0] == 0x56);
    assert(value.legacy_axes[1] == 0x78);
    assert(!value.release_pending);

    report = make_report(0, 0xab, 0x90);
    assert(usb_playstation_wheel_value_apply(&value, &report, 700));
    assert(value.legacy_axes[0] == 0x56);
    assert(value.legacy_axes[1] == 0x78);
}

static void test_sets_protocol_value_directly(void) {
    UsbPlaystationWheelValue value;
    usb_playstation_wheel_value_init(&value);

    usb_playstation_wheel_value_set(&value, 0x34, 0x12, 500);

    assert(value.legacy_axes[0] == 0x12);
    assert(value.legacy_axes[1] == 0x34);
    assert(value.deadline_ms == 3500);
    assert(!value.release_pending);
    UsbDeviceOutputReport report = make_report(0, 0x78, 0x56);
    assert(usb_playstation_wheel_value_apply(&value, &report, 600));
    assert(value.legacy_axes[0] == 0x12);
    assert(value.legacy_axes[1] == 0x34);
    usb_playstation_wheel_value_set(NULL, 0, 0, 0);
}

static void test_expires_strictly_after_deadline(void) {
    UsbPlaystationWheelValue value;
    usb_playstation_wheel_value_init(&value);
    UsbDeviceOutputReport report = make_report(1, 0x34, 0x12);
    assert(usb_playstation_wheel_value_apply(&value, &report, 500));

    assert(!usb_playstation_wheel_value_expire(&value, 3500));
    assert(value.legacy_axes[0] == 0x12);
    assert(usb_playstation_wheel_value_expire(&value, 3501));
    assert(value.legacy_axes[0] == 0);
    assert(value.legacy_axes[1] == 0);
    assert(value.release_pending);
    assert(!usb_playstation_wheel_value_expire(&value, 3502));
}

static void test_rejects_other_reports(void) {
    UsbPlaystationWheelValue value;
    usb_playstation_wheel_value_init(&value);
    UsbDeviceOutputReport report = make_report(1, 0x34, 0x12);

    report.report_id = 6;
    assert(!usb_playstation_wheel_value_apply(&value, &report, 0));
    report.report_id = 5;
    report.length = 6;
    assert(!usb_playstation_wheel_value_apply(&value, &report, 0));
    report.length = USB_DEVICE_REPORT_SIZE;
    report.report_type = USB_DEVICE_HID_REPORT_FEATURE;
    assert(!usb_playstation_wheel_value_apply(&value, &report, 0));
    assert(!usb_playstation_wheel_value_apply(NULL, &report, 0));
    assert(!usb_playstation_wheel_value_apply(&value, NULL, 0));
    assert(usb_playstation_wheel_value_axes(NULL) == NULL);
}

static void test_persistently_copies_attached_wheel_axes(void) {
    UsbPlaystationWheelValue value;
    usb_playstation_wheel_value_init(&value);
    const uint8_t axes[] = {0x12, 0x34};

    assert(!usb_playstation_wheel_value_copy_axes(&value, axes));
    usb_playstation_wheel_value_set_axis_copy(&value, true);
    assert(usb_playstation_wheel_value_copy_axes(&value, axes));
    assert(value.legacy_axes[0] == 0x34);
    assert(value.legacy_axes[1] == 0x12);

    usb_playstation_wheel_value_set_axis_copy(&value, false);
    assert(!usb_playstation_wheel_value_copy_axes(&value, axes));
    assert(!usb_playstation_wheel_value_copy_axes(NULL, axes));
    assert(!usb_playstation_wheel_value_copy_axes(&value, NULL));
}

int main(void) {
    test_applies_asserted_value_in_wheel_order();
    test_applies_one_release_value();
    test_sets_protocol_value_directly();
    test_expires_strictly_after_deadline();
    test_rejects_other_reports();
    test_persistently_copies_attached_wheel_axes();
    return 0;
}
