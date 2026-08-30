#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
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

static void test_configuration_changes_during_setup(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    device.alternate_interfaces[0] = 3;
    device.alternate_interfaces[1] = 4;
    UsbControlRequest control = request(USB_CONTROL_SET_CONFIGURATION);
    control.value = 0x0107;

    usb_device_control_handle(&device, &control, &catalog, false);
    assert(usb_device_control_configured(&device));
    assert(device.alternate_interfaces[0] == 0 && device.alternate_interfaces[1] == 0);

    control = request(USB_CONTROL_GET_CONFIGURATION);
    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE);
    assert(transfer.value == 7 && transfer.length == 1);
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

    control.descriptor_type = 0x21;
    control.descriptor_index = 0;
    control.recipient = 1;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_STALL);

    device.configuration = 1;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_DATA && transfer.data.data == hid_descriptor);

    device.configuration = 0;
    control.descriptor_type = 0x23;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.length == 0);
}

static void test_hid_state_and_report_handoff(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_HID_GET_PROTOCOL);
    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.value == 0);

    control = request(USB_CONTROL_HID_GET_IDLE);
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.value == 0);

    control = request(USB_CONTROL_HID_SET_IDLE);
    control.value = 0x0700;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control = request(USB_CONTROL_HID_GET_IDLE);
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.value == 7);

    control = request(USB_CONTROL_HID_SET_PROTOCOL);
    control.value = 7;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control = request(USB_CONTROL_HID_GET_PROTOCOL);
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.value == 7);

    control = request(USB_CONTROL_HID_SET_REPORT);
    control.value = 0x0201;
    control.length = 8;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_REPORT_OUT);
    assert(transfer.report_type == 2 && transfer.report_id == 1 && transfer.length == 8);
}

static void test_tracks_both_alternate_interfaces(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, true, false);
    UsbControlRequest control = request(USB_CONTROL_SET_INTERFACE);
    control.index = 1;
    control.value = 7;

    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_ACKNOWLEDGE);

    control = request(USB_CONTROL_GET_INTERFACE);
    control.index = 1;
    UsbControlTransfer transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE);
    assert(transfer.value == 7 && transfer.length == 1);

    control.index = 0;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_VALUE && transfer.value == 0);

    control.index = 2;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
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

static void test_rejects_malformed_descriptors_and_endpoints(void) {
    UsbDeviceControl device;
    usb_device_control_init(&device, false, false);
    UsbControlRequest control = request(USB_CONTROL_GET_DESCRIPTOR);
    UsbControlTransfer transfer;

    control.descriptor_type = 1;
    control.recipient = 1;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.recipient = 0;
    control.descriptor_index = 1;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.descriptor_type = 2;
    control.descriptor_index = 0;
    control.recipient = 1;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.descriptor_type = 3;
    control.recipient = 0;
    control.descriptor_index = 0;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_DATA);
    control.descriptor_type = 0x22;
    control.recipient = 1;
    control.descriptor_index = 0;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    device.configuration = 1;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_DATA && transfer.data.data == report_descriptor);
    control.descriptor_type = 0x23;
    control.descriptor_index = 1;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.descriptor_type = UINT8_MAX;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);

    UsbDescriptorCatalog empty = catalog;
    empty.device.data = NULL;
    control.descriptor_type = 1;
    control.descriptor_index = 0;
    control.recipient = 0;
    assert(usb_device_control_handle(&device, &control, &empty, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    empty.device.data = device_descriptor;
    empty.device.length = 0;
    assert(usb_device_control_handle(&device, &control, &empty, false).kind ==
           USB_CONTROL_TRANSFER_STALL);

    control = request(USB_CONTROL_GET_STATUS);
    control.recipient = 3;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.recipient = 2;
    control.index = 0x10;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.index = 5;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.index = 0x84;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_VALUE);

    control = request(USB_CONTROL_SET_FEATURE);
    control.recipient = 0;
    control.value = 2;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control.recipient = 2;
    control.value = 0;
    control.index = 0;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);

    control = request(USB_CONTROL_SET_INTERFACE);
    control.index = USB_DEVICE_INTERFACE_COUNT;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
    control = request(USB_CONTROL_HID_GET_REPORT);
    control.value = 0x0302;
    control.length = 64;
    transfer = usb_device_control_handle(&device, &control, &catalog, false);
    assert(transfer.kind == USB_CONTROL_TRANSFER_REPORT_IN && transfer.report_type == 3 &&
           transfer.report_id == 2 && transfer.length == 64);

    device.configuration = 1;
    control = request(USB_CONTROL_SET_ADDRESS);
    control.value = 0;
    (void)usb_device_control_handle(&device, &control, &catalog, false);
    usb_device_control_complete(&device);
    assert(!usb_device_control_configured(&device));
    control.kind = (UsbControlRequestKind)UINT8_MAX;
    assert(usb_device_control_handle(&device, &control, &catalog, false).kind ==
           USB_CONTROL_TRANSFER_STALL);
}

int main(void) {
    test_address_changes_after_status_stage();
    test_new_setup_cancels_pending_change();
    test_configuration_changes_during_setup();
    test_selects_and_clips_descriptors();
    test_hid_state_and_report_handoff();
    test_tracks_both_alternate_interfaces();
    test_device_status();
    test_remote_wakeup_feature();
    test_forces_xbox_wakeup_status();
    test_endpoint_halt_feature();
    test_rejects_malformed_descriptors_and_endpoints();
    return 0;
}
