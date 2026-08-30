#include "usb/device_control.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_DESCRIPTOR_DEVICE = 1,
    USB_DESCRIPTOR_CONFIGURATION = 2,
    USB_DESCRIPTOR_STRING = 3,
    USB_DESCRIPTOR_HID = 0x21,
    USB_DESCRIPTOR_HID_REPORT = 0x22,
    USB_RECIPIENT_DEVICE = 0,
    USB_RECIPIENT_INTERFACE = 1,
    USB_RECIPIENT_ENDPOINT = 2,
    USB_FEATURE_ENDPOINT_HALT = 0,
    USB_FEATURE_DEVICE_REMOTE_WAKEUP = 1,
    USB_ENDPOINT_NUMBER_MASK = 0x0f,
    USB_ENDPOINT_ADDRESS_RESERVED_MASK = 0xff70,
    USB_ENDPOINT_COUNT = 5,
};

static UsbControlTransfer stall(void) {
    return (UsbControlTransfer){.kind = USB_CONTROL_TRANSFER_STALL};
}

static UsbControlTransfer acknowledge(void) {
    return (UsbControlTransfer){.kind = USB_CONTROL_TRANSFER_ACKNOWLEDGE};
}

static UsbControlTransfer value(uint16_t response, uint16_t length) {
    return (UsbControlTransfer){
        .kind = USB_CONTROL_TRANSFER_VALUE,
        .value = response,
        .length = length,
    };
}

static UsbControlTransfer data(UsbDescriptorView descriptor, uint16_t requested_length) {
    if (descriptor.data == 0 || descriptor.length == 0) {
        return stall();
    }
    uint16_t length = descriptor.length > requested_length ? requested_length : descriptor.length;
    return (UsbControlTransfer){
        .kind = USB_CONTROL_TRANSFER_DATA,
        .data = descriptor,
        .length = length,
    };
}

void usb_device_control_init(UsbDeviceControl *device, bool self_powered,
                             bool remote_wakeup_forced) {
    *device = (UsbDeviceControl){
        .hid_protocol = 1,
        .self_powered = self_powered,
        .remote_wakeup_forced = remote_wakeup_forced,
    };
}

void usb_device_control_cancel(UsbDeviceControl *device) {
    device->pending_change = USB_DEVICE_PENDING_NONE;
    device->pending_value = 0;
}

static UsbControlTransfer get_descriptor(const UsbControlRequest *request,
                                         const UsbDescriptorCatalog *catalog) {
    switch (request->descriptor_type) {
    case USB_DESCRIPTOR_DEVICE:
        return request->recipient == USB_RECIPIENT_DEVICE && request->descriptor_index == 0
                   ? data(catalog->device, request->length)
                   : stall();
    case USB_DESCRIPTOR_CONFIGURATION:
        return request->recipient == USB_RECIPIENT_DEVICE && request->descriptor_index == 0
                   ? data(catalog->configuration, request->length)
                   : stall();
    case USB_DESCRIPTOR_STRING:
        return request->recipient == USB_RECIPIENT_DEVICE &&
                       request->descriptor_index < catalog->string_count
                   ? data(catalog->strings[request->descriptor_index], request->length)
                   : stall();
    case USB_DESCRIPTOR_HID:
        return request->recipient == USB_RECIPIENT_INTERFACE && request->descriptor_index == 0
                   ? data(catalog->hid, request->length)
                   : stall();
    case USB_DESCRIPTOR_HID_REPORT:
        return request->recipient == USB_RECIPIENT_INTERFACE && request->descriptor_index == 0
                   ? data(catalog->report, request->length)
                   : stall();
    default:
        return stall();
    }
}

static UsbControlTransfer hid_report(const UsbControlRequest *request, bool input) {
    return (UsbControlTransfer){
        .kind = input ? USB_CONTROL_TRANSFER_REPORT_IN : USB_CONTROL_TRANSFER_REPORT_OUT,
        .length = request->length,
        .report_type = (uint8_t)(request->value >> 8),
        .report_id = (uint8_t)request->value,
    };
}

/**
 * @brief Tests whether a standard request selects a supported endpoint.
 *
 * Accepts endpoint numbers zero through four when control is permitted, accepts endpoints one
 * through four otherwise, permits the input-direction bit, and rejects every reserved address bit.
 *
 * @param[in] request Classified standard endpoint request.
 * @param[in] include_control True to permit endpoint zero.
 * @return True when the endpoint address is supported; otherwise false.
 */
static bool supported_endpoint(const UsbControlRequest *request, bool include_control) {
    uint8_t endpoint = (uint8_t)request->index & USB_ENDPOINT_NUMBER_MASK;
    return request->recipient == USB_RECIPIENT_ENDPOINT &&
           (request->index & USB_ENDPOINT_ADDRESS_RESERVED_MASK) == 0 &&
           (include_control || endpoint != 0) && endpoint < USB_ENDPOINT_COUNT;
}

UsbControlTransfer usb_device_control_handle(UsbDeviceControl *device,
                                             const UsbControlRequest *request,
                                             const UsbDescriptorCatalog *catalog,
                                             bool endpoint_halted) {
    switch (request->kind) {
    case USB_CONTROL_GET_STATUS:
        if (request->recipient == USB_RECIPIENT_DEVICE) {
            return value((device->self_powered ? 1 : 0) |
                             (device->remote_wakeup || device->remote_wakeup_forced ? 2 : 0),
                         2);
        }
        if (request->recipient == USB_RECIPIENT_INTERFACE) {
            return value(0, 2);
        }
        if (supported_endpoint(request, true)) {
            return value(endpoint_halted ? 1 : 0, 2);
        }
        return stall();
    case USB_CONTROL_CLEAR_FEATURE:
    case USB_CONTROL_SET_FEATURE:
        if (request->recipient == USB_RECIPIENT_DEVICE &&
            request->value == USB_FEATURE_DEVICE_REMOTE_WAKEUP) {
            device->remote_wakeup = request->kind == USB_CONTROL_SET_FEATURE;
            return acknowledge();
        }
        if (request->value == USB_FEATURE_ENDPOINT_HALT && usb_device_control_configured(device) &&
            supported_endpoint(request, false)) {
            return acknowledge();
        }
        return stall();
    case USB_CONTROL_SET_ADDRESS:
        device->pending_change = USB_DEVICE_PENDING_ADDRESS;
        device->pending_value = (uint8_t)request->value;
        return acknowledge();
    case USB_CONTROL_GET_DESCRIPTOR:
        return get_descriptor(request, catalog);
    case USB_CONTROL_GET_CONFIGURATION:
        return value(device->configuration, 1);
    case USB_CONTROL_SET_CONFIGURATION:
        device->pending_change = USB_DEVICE_PENDING_CONFIGURATION;
        device->pending_value = (uint8_t)request->value;
        return acknowledge();
    case USB_CONTROL_GET_INTERFACE:
        return value(device->alternate_interface, 1);
    case USB_CONTROL_SET_INTERFACE:
        if (request->value != 0) {
            return stall();
        }
        device->alternate_interface = 0;
        return acknowledge();
    case USB_CONTROL_HID_GET_REPORT:
        return hid_report(request, true);
    case USB_CONTROL_HID_SET_REPORT:
        return hid_report(request, false);
    case USB_CONTROL_HID_GET_IDLE:
        return value(device->hid_idle_rate, 1);
    case USB_CONTROL_HID_SET_IDLE:
        device->hid_idle_rate = (uint8_t)(request->value >> 8);
        return acknowledge();
    case USB_CONTROL_HID_GET_PROTOCOL:
        return value(device->hid_protocol, 1);
    case USB_CONTROL_HID_SET_PROTOCOL:
        device->hid_protocol = (uint8_t)request->value;
        return acknowledge();
    default:
        return stall();
    }
}

void usb_device_control_complete(UsbDeviceControl *device) {
    if (device->pending_change == USB_DEVICE_PENDING_ADDRESS) {
        device->address = device->pending_value;
        if (device->address == 0) {
            device->configuration = 0;
        }
    } else if (device->pending_change == USB_DEVICE_PENDING_CONFIGURATION) {
        device->configuration = device->pending_value;
    }
    device->pending_change = USB_DEVICE_PENDING_NONE;
    device->pending_value = 0;
}

bool usb_device_control_configured(const UsbDeviceControl *device) {
    return device->configuration != 0;
}
