#ifndef OPENTEC_BASE_USB_CONSOLE_DESCRIPTOR_H
#define OPENTEC_BASE_USB_CONSOLE_DESCRIPTOR_H

#include <stdint.h>

#include "board/identity.h"
#include "usb/descriptor.h"

enum {
    USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE = 32,
    USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE = 41,
    USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE = 160,
};

UsbDeviceIdentity usb_xbox_gip_device_identity(uint16_t product_id);
UsbDeviceIdentity usb_playstation_device_identity(BoardVariant variant);
const char *usb_xbox_gip_product_name(BoardVariant variant);
const char *usb_playstation_product_name(BoardVariant variant);
const char *usb_xbox_gip_initial_serial(void);
void usb_xbox_gip_configuration_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE]);
void usb_playstation_configuration_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE]);
void usb_playstation_report_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE]);

#endif
