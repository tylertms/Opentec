#ifndef OPENTEC_BASE_USB_UPDATER_PROTOCOL_H
#define OPENTEC_BASE_USB_UPDATER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_UPDATER_DEVICE_IDENTITY_SIZE = 4,
    USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE = 6,
};

/** @brief Operation selected by one motor-updater USB packet. */
typedef enum {
    USB_UPDATER_REQUEST_NONE,
    USB_UPDATER_REQUEST_BRIDGE,
    USB_UPDATER_REQUEST_DEVICE_INFO,
    USB_UPDATER_REQUEST_RESET,
} UsbUpdaterRequestKind;

/** @brief Decoded updater operation and retained packet view. */
typedef struct {
    const uint8_t *data;
    uint8_t length;
    UsbUpdaterRequestKind kind;
} UsbUpdaterRequest;

bool usb_updater_protocol_decode(const uint8_t *data, uint8_t length, UsbUpdaterRequest *request);
void usb_updater_protocol_encode_device_info(
    const uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE],
    uint8_t response[USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE]);

#endif
