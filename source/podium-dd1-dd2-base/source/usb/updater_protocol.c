#include "usb/updater_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    USB_UPDATER_BRIDGE_COMMAND = 0x5a,
    USB_UPDATER_CONTROL_COMMAND = 0xf8,
    USB_UPDATER_DEVICE_INFO_OPERATION = 0x01,
    USB_UPDATER_RESET_OPERATION = 0x09,
    USB_UPDATER_RESET_ENABLED = 0x01,
    USB_UPDATER_RESET_MAGIC = 0xfe,
};

/**
 * @brief Decodes one motor-updater USB request.
 *
 * Selects marker 0x5A bridge packets, the F8/01 device-information query, or the guarded
 * F8/09/01/FE reset operation. Unsupported and truncated packets are rejected.
 *
 * @param[in] data Complete updater packet bytes.
 * @param[in] length Number of packet bytes from one through 63.
 * @param[out] request Decoded operation and packet view.
 * @return True when the packet selects a supported updater operation; otherwise false.
 */
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

/**
 * @brief Encodes the motor-updater device-information response.
 *
 * Prefixes the selected four-character runtime identity with the F8/01 control response header.
 *
 * @param[in] identity Four-character runtime identity.
 * @param[out] response Six-byte updater response.
 */
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
