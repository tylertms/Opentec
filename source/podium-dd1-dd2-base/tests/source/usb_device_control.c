#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb/device_control.h"

static const uint8_t device_descriptor[] = {18, 1, 0, 2};
static const uint8_t configuration_descriptor[] = {9, 2, 41, 0};
static const uint8_t hid_descriptor[] = {9, 0x21, 0x11, 1};
static const uint8_t report_descriptor[] = {5, 1, 9, 4};
static const uint8_t language_descriptor[] = {4, 3, 9, 4};
static const UsbDescriptorView strings[] = {
    {.data = language_descriptor, .length = sizeof(language_descriptor)},
};
static const UsbDescriptorCatalog catalog = {
    .device = {.data = device_descriptor, .length = sizeof(device_descriptor)},
    .configuration =
        {
            .data = configuration_descriptor,
            .length = sizeof(configuration_descriptor),
        },
    .hid = {.data = hid_descriptor, .length = sizeof(hid_descriptor)},
    .report = {.data = report_descriptor, .length = sizeof(report_descriptor)},
    .strings = strings,
    .string_count = 1,
};

static UsbControlRequest request(UsbControlRequestKind kind) {
    return (UsbControlRequest){.kind = kind};
}

static void test_address_changes_after_status_stage(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_SET_ADDRESS);
    control.value = 42;

    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);

    assert(transfer.kind == USB_CONTROL_TRANSFER_ACKNOWLEDGE);
    assert(device.address == 0);
    usb_device_control_complete(&device);
    assert(device.address == 42);
}

static void test_new_setup_cancels_pending_change(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_SET_ADDRESS);
    control.value = 42;

    usb_device_control_handle(&device, &control, &catalog, false);
    usb_device_control_cancel(&device);
    usb_device_control_complete(&device);

    assert(device.address == 0);
}

static void test_configuration_changes_after_status_stage(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_SET_CONFIGURATION);
    control.value = 1;

    usb_device_control_handle(&device, &control, &catalog, false);
    assert(!usb_device_control_configured(&device));
    usb_device_control_complete(&device);
    assert(usb_device_control_configured(&device));

    control = request(USB_CONTROL_GET_CONFIGURATION);
    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE);
    assert(transfer.value == 1 && transfer.length == 1);
}

static void test_selects_and_clips_descriptors(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_GET_DESCRIPTOR);
    control.descriptor_type = 1;
    control.length = 2;
    control.recipient = 0;

    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);

    assert(transfer.kind == USB_CONTROL_TRANSFER_DATA);
    assert(transfer.data.data == device_descriptor);
    assert(transfer.data.length == sizeof(device_descriptor));
    assert(transfer.length == 2);

    control.descriptor_type = 3;
    control.descriptor_index = 1;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_STALL);
}

static void test_hid_state_and_report_handoff(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_HID_SET_IDLE);
    control.value = 0x0700;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control = request(USB_CONTROL_HID_GET_IDLE);
    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.value == 7);

    control = request(USB_CONTROL_HID_SET_REPORT);
    control.value = 0x0201;
    control.length = 8;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_REPORT_OUT);
    assert(transfer.report_type == 2 && transfer.report_id == 1 && transfer.length == 8);
}

static void test_device_status(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    device.remote_wakeup = true;
    UsbControlRequest control = request(USB_CONTROL_GET_STATUS);
    control.recipient = 0;

    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);

    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE);
    assert(transfer.value == 3 && transfer.length == 2);

    control.recipient = 1;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.value == 0);
}

static void test_remote_wakeup_feature(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_SET_FEATURE);
    control.value = 1;

    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control = request(USB_CONTROL_GET_STATUS);
    assert(usb_device_control_handle(&device, &control, &catalog, false).value == 3);

    control = request(USB_CONTROL_CLEAR_FEATURE);
    control.value = 1;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control = request(USB_CONTROL_GET_STATUS);
    assert(usb_device_control_handle(&device, &control, &catalog, false).value == 1);
}

static void test_forces_xbox_wakeup_status(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, true);
    UsbControlRequest control = request(USB_CONTROL_CLEAR_FEATURE);
    control.value = 1;
    (void)usb_device_control_handle(&device, &control, &catalog, false);

    control = request(USB_CONTROL_GET_STATUS);
    assert(usb_device_control_handle(&device, &control, &catalog, false).value == 3);
}

static void test_endpoint_halt_feature(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_SET_FEATURE);
    control.recipient = 2;
    control.index = 0x81;

    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);

    device.configuration = 1;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control = request(USB_CONTROL_GET_STATUS);
    control.recipient = 2;
    control.index = 0x81;
    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, true);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE);
    assert(transfer.value == 1 && transfer.length == 2);

    control.index = 0;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.value == 0);

    control = request(USB_CONTROL_CLEAR_FEATURE);
    control.recipient = 2;
    control.index = 0x81;
    assert(usb_device_control_handle(&device, &control, &catalog, true).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control.index = 5;
    assert(usb_device_control_handle(&device, &control, &catalog, true).kind ==
           USB_CONTROL_TRANSFER_STALL);
}

int main(void) {
    test_address_changes_after_status_stage();
    test_new_setup_cancels_pending_change();
    test_configuration_changes_after_status_stage();
    test_selects_and_clips_descriptors();
    test_hid_state_and_report_handoff();
    test_device_status();
    test_remote_wakeup_feature();
    test_forces_xbox_wakeup_status();
    test_endpoint_halt_feature();
    return 0;
}
