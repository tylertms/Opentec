#include "usb/device_control.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Descriptor, recipient, feature, and endpoint constants for USB control handling. */
enum {
    USB_DESCRIPTOR_DEVICE = 1,                   /**< USB device descriptor type. */
    USB_DESCRIPTOR_CONFIGURATION = 2,            /**< USB configuration descriptor type. */
    USB_DESCRIPTOR_STRING = 3,                   /**< USB string descriptor type. */
    USB_DESCRIPTOR_HID = 0x21,                   /**< HID class descriptor type. */
    USB_DESCRIPTOR_HID_REPORT = 0x22,            /**< HID report descriptor type. */
    USB_DESCRIPTOR_HID_PHYSICAL = 0x23,          /**< HID physical descriptor type. */
    USB_RECIPIENT_DEVICE = 0,                    /**< Device request recipient. */
    USB_RECIPIENT_INTERFACE = 1,                 /**< Interface request recipient. */
    USB_RECIPIENT_ENDPOINT = 2,                  /**< Endpoint request recipient. */
    USB_FEATURE_ENDPOINT_HALT = 0,               /**< Endpoint halt feature selector. */
    USB_FEATURE_DEVICE_REMOTE_WAKEUP = 1,        /**< Device remote-wakeup feature selector. */
    USB_ENDPOINT_NUMBER_MASK = 0x0f,             /**< Endpoint-number bit mask. */
    USB_ENDPOINT_ADDRESS_RESERVED_MASK = 0xff70, /**< Reserved endpoint-address bit mask. */
    USB_ENDPOINT_COUNT = 5,                      /**< Number of supported endpoint numbers. */
};

/**
 * @brief Builds a stalled endpoint-zero transfer.
 *
 * Selects the transfer result used for malformed or unsupported requests.
 *
 * @return Stalled control-transfer description.
 */
static UsbControlTransfer stall(void) {
    return (UsbControlTransfer){.kind = USB_CONTROL_TRANSFER_STALL};
}

/**
 * @brief Builds an acknowledged endpoint-zero transfer.
 *
 * Selects an empty status-stage response for a completed host-to-device request.
 *
 * @return Acknowledged control-transfer description.
 */
static UsbControlTransfer acknowledge(void) {
    return (UsbControlTransfer){.kind = USB_CONTROL_TRANSFER_ACKNOWLEDGE};
}

/**
 * @brief Builds a short endpoint-zero value response.
 *
 * Retains a one- or two-byte response value and its transfer length.
 *
 * @param[in] response Value returned low byte first.
 * @param[in] length Number of response bytes.
 * @return Value control-transfer description.
 */
static UsbControlTransfer value(uint16_t response, uint16_t length) {
    return (UsbControlTransfer){
        .kind = USB_CONTROL_TRANSFER_VALUE,
        .value = response,
        .length = length,
    };
}

/**
 * @brief Builds an endpoint-zero descriptor response.
 *
 * Limits valid descriptor data to the host-requested length and stalls empty or null descriptor
 * views. A physical descriptor is handled separately as its valid zero-length VALUE response.
 *
 * @param[in] descriptor Descriptor bytes and available length.
 * @param[in] requested_length Maximum length requested by the host.
 * @return Data transfer limited to the request, or a stalled transfer for an empty descriptor.
 */
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
        .self_powered = self_powered,
        .remote_wakeup_forced = remote_wakeup_forced,
    };
}

/**
 * @brief Selects a descriptor response for endpoint zero.
 *
 * Serves device, configuration, and string descriptors by index, gates HID and report descriptors
 * on an active configuration, and completes physical-descriptor requests with an empty data stage.
 * An active string alias hides its target from ordinary descriptor-index requests.
 *
 * @param[in] request Classified descriptor request.
 * @param[in] catalog Active descriptor catalog.
 * @param[in] configured True when configuration one is active.
 * @return Selected descriptor transfer or a stall for an unsupported request.
 */
static UsbControlTransfer get_descriptor(const UsbControlRequest *request,
                                         const UsbDescriptorCatalog *catalog, bool configured) {
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
        if (request->recipient != USB_RECIPIENT_DEVICE) {
            return stall();
        }
        if (catalog->string_alias_valid && request->descriptor_index == catalog->string_alias &&
            catalog->string_alias_target < catalog->string_count) {
            return data(catalog->strings[catalog->string_alias_target], request->length);
        }
        if (catalog->string_alias_valid &&
            request->descriptor_index == catalog->string_alias_target) {
            return stall();
        }
        return request->descriptor_index < catalog->string_count
                   ? data(catalog->strings[request->descriptor_index], request->length)
                   : stall();
    case USB_DESCRIPTOR_HID:
        return configured && request->recipient == USB_RECIPIENT_INTERFACE &&
                       request->descriptor_index == 0 && request->index == 0
                   ? data(catalog->hid, request->length)
                   : stall();
    case USB_DESCRIPTOR_HID_REPORT:
        return configured && request->recipient == USB_RECIPIENT_INTERFACE &&
                       request->descriptor_index == 0 && request->index == 0
                   ? data(catalog->report, request->length)
                   : stall();
    case USB_DESCRIPTOR_HID_PHYSICAL:
        return request->recipient == USB_RECIPIENT_INTERFACE && request->descriptor_index == 0 &&
                       request->index == 0
                   ? value(0, 0)
                   : stall();
    default:
        return stall();
    }
}

/**
 * @brief Builds a HID report control transfer.
 *
 * Retains the requested report type, report identifier, direction, and maximum transfer length.
 *
 * @param[in] request Classified HID report request.
 * @param[in] input True for a device-to-host report; false for a host-to-device report.
 * @return HID report control-transfer description.
 */
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
        return get_descriptor(request, catalog, usb_device_control_configured(device));
    case USB_CONTROL_GET_CONFIGURATION:
        return value(device->configuration, 1);
    case USB_CONTROL_SET_CONFIGURATION:
        device->configuration = (uint8_t)request->value;
        for (uint8_t index = 0; index < USB_DEVICE_INTERFACE_COUNT; index++) {
            device->alternate_interfaces[index] = 0;
        }
        return acknowledge();
    case USB_CONTROL_GET_INTERFACE:
        return request->index < USB_DEVICE_INTERFACE_COUNT
                   ? value(device->alternate_interfaces[request->index], 1)
                   : stall();
    case USB_CONTROL_SET_INTERFACE:
        if (request->index >= USB_DEVICE_INTERFACE_COUNT) {
            return stall();
        }
        device->alternate_interfaces[request->index] = (uint8_t)request->value;
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
    }
    device->pending_change = USB_DEVICE_PENDING_NONE;
    device->pending_value = 0;
}

bool usb_device_control_configured(const UsbDeviceControl *device) {
    return device->configuration != 0;
}
