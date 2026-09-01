#include "usb/updater_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Wire markers and operation values accepted by the updater protocol decoder. */
enum {
    USB_UPDATER_BRIDGE_COMMAND = 0x5a,        /**< Marker for a bridged updater request. */
    USB_UPDATER_CONTROL_COMMAND = 0xf8,       /**< Marker for an updater control request. */
    USB_UPDATER_DEVICE_INFO_OPERATION = 0x01, /**< Device-information control operation. */
    USB_UPDATER_RESET_OPERATION = 0x09,       /**< Reset control operation. */
    USB_UPDATER_RESET_ENABLED = 0x01,         /**< Required reset enable byte. */
    USB_UPDATER_RESET_MAGIC = 0xfe,           /**< Required reset guard byte. */
};

bool usb_updater_protocol_decode(const uint8_t *data, uint8_t length, UsbUpdaterRequest *request) {
    if (data == NULL || request == NULL || length == 0 || length > 63) {
        return false;
    }

    *request = (UsbUpdaterRequest){0};
    if (data[0] == USB_UPDATER_BRIDGE_COMMAND && length >= 2) {
        request->data = data;
        request->length = length;
        request->kind = USB_UPDATER_REQUEST_BRIDGE;
        return true;
    }
    if (data[0] != USB_UPDATER_CONTROL_COMMAND || length < 2) {
        return false;
    }
    if (data[1] == USB_UPDATER_DEVICE_INFO_OPERATION) {
        request->kind = USB_UPDATER_REQUEST_DEVICE_INFO;
        return true;
    }
    if (data[1] != USB_UPDATER_RESET_OPERATION || length < 4 ||
        data[2] != USB_UPDATER_RESET_ENABLED || data[3] != USB_UPDATER_RESET_MAGIC) {
        return false;
    }
    request->kind = USB_UPDATER_REQUEST_RESET;
    return true;
}

void usb_updater_protocol_encode_device_info(
    const uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE],
    uint8_t response[USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE]) {
    if (identity == NULL || response == NULL) {
        return;
    }
    response[0] = USB_UPDATER_CONTROL_COMMAND;
    response[1] = USB_UPDATER_DEVICE_INFO_OPERATION;
    for (uint8_t index = 0; index < USB_UPDATER_DEVICE_IDENTITY_SIZE; index++) {
        response[index + 2] = identity[index];
    }
}
