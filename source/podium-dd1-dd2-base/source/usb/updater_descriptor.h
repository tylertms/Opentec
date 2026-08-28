#ifndef OPENTEC_BASE_USB_UPDATER_DESCRIPTOR_H
#define OPENTEC_BASE_USB_UPDATER_DESCRIPTOR_H

#include <stdint.h>

#include "usb/descriptor.h"

enum { USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE = 67 };

UsbDeviceIdentity usb_updater_device_identity(void);
const char *usb_updater_product_name(void);
void usb_updater_configuration_descriptor_encode(
    uint8_t output[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE]);

#endif
