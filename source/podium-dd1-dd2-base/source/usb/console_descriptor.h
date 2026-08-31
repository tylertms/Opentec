#ifndef OPENTEC_BASE_USB_CONSOLE_DESCRIPTOR_H
#define OPENTEC_BASE_USB_CONSOLE_DESCRIPTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/descriptor.h"

enum {
    USB_XBOX_GIP_DIGEST_SIZE = 8,
    USB_XBOX_GIP_SERIAL_TEXT_SIZE = USB_XBOX_GIP_DIGEST_SIZE * 2,
    USB_XBOX_GIP_SERIAL_SIZE = USB_XBOX_GIP_SERIAL_TEXT_SIZE + 1,
    USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE = 32,
    USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE = 40,
    USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE = 18,
    USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE = 41,
    USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE = 160,
};

UsbDeviceIdentity usb_xbox_gip_device_identity(uint16_t product_id);
UsbDeviceIdentity usb_playstation_device_identity(BoardVariant variant);
UsbDeviceIdentity usb_playstation_device_identity_for_mode(BoardVariant variant,
                                                           uint8_t wheel_mode);
uint8_t usb_xbox_gip_mode_code(BoardVariant variant, uint8_t wheel_mode);
bool usb_xbox_gip_product_id(BoardVariant variant, uint8_t wheel_mode, uint16_t *product_id);
const char *usb_xbox_gip_product_name(BoardVariant variant);
const char *usb_playstation_product_name(BoardVariant variant);
const char *usb_xbox_gip_initial_serial(void);
void usb_xbox_gip_serial_encode(const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                char output[USB_XBOX_GIP_SERIAL_SIZE]);
void usb_xbox_gip_configuration_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE]);
void usb_xbox_gip_security_descriptor_encode(uint8_t output[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE]);
void usb_xbox_gip_os_string_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE]);
void usb_playstation_configuration_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE]);
void usb_playstation_report_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE]);

#endif
