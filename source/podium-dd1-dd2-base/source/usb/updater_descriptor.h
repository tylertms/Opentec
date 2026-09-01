#ifndef OPENTEC_BASE_USB_UPDATER_DESCRIPTOR_H
#define OPENTEC_BASE_USB_UPDATER_DESCRIPTOR_H

#include <stdint.h>

#include "usb/descriptor.h"

/** @brief Updater USB configuration descriptor constants. */
enum {
    USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE =
        67 /**< Number of bytes in the configuration descriptor. */
};

/**
 * @brief Returns the USB identity used by the motor updater transport.
 *
 * Describes the compound communications-class updater device with product ID 0x0718 and product
 * string index 7.
 *
 * @return Motor updater USB identity.
 */
UsbDeviceIdentity usb_updater_device_identity(void);

/**
 * @brief Returns the motor updater USB product name.
 *
 * Supplies the product text referenced by string descriptor index 7.
 *
 * @return Null-terminated updater product name.
 */
const char *usb_updater_product_name(void);

/**
 * @brief Encodes the motor updater USB configuration descriptor.
 *
 * Emits the two-interface CDC ACM configuration with interrupt endpoint 0x82 and bulk endpoints
 * 0x03 and 0x83.
 *
 * @param[out] output Destination for the 67-byte configuration descriptor.
 */
void usb_updater_configuration_descriptor_encode(
    uint8_t output[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE]);

#endif
