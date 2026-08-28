#include "usb/updater_descriptor.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Returns the USB identity used by the motor updater transport.
 *
 * Describes the compound communications-class updater device with product ID 0x0718 and product
 * string index 7.
 *
 * @return Motor updater USB identity.
 */
UsbDeviceIdentity usb_updater_device_identity(void) {
    return (UsbDeviceIdentity){
        .usb_version = 0x0200,
        .device_class = 0x02,
        .device_subclass = 0x00,
        .device_protocol = 0x00,
        .vendor_id = 0x0eb7,
        .product_id = 0x0718,
        .device_version = 0x0001,
        .control_packet_size = 64,
        .manufacturer_string = 1,
        .product_string = 7,
    };
}

/**
 * @brief Returns the motor updater USB product name.
 *
 * Supplies the product text referenced by string descriptor index 7.
 *
 * @return Null-terminated updater product name.
 */
const char *usb_updater_product_name(void) { return "FANATEC EBLDC Updater"; }

/**
 * @brief Encodes the motor updater USB configuration descriptor.
 *
 * Emits the two-interface CDC ACM configuration with interrupt endpoint 0x82 and bulk endpoints
 * 0x03 and 0x83.
 *
 * @param[out] output Destination for the 67-byte configuration descriptor.
 */
void usb_updater_configuration_descriptor_encode(
    uint8_t output[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE]) {
    static const uint8_t descriptor[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0xc0, 0x32, 0x09, 0x04, 0x00, 0x00, 0x01,
        0x02, 0x02, 0x01, 0x00, 0x05, 0x24, 0x00, 0x10, 0x01, 0x04, 0x24, 0x02, 0x02, 0x05,
        0x24, 0x06, 0x00, 0x01, 0x05, 0x24, 0x01, 0x00, 0x01, 0x07, 0x05, 0x82, 0x03, 0x08,
        0x00, 0x02, 0x09, 0x04, 0x01, 0x00, 0x02, 0x0a, 0x00, 0x00, 0x00, 0x07, 0x05, 0x03,
        0x02, 0x40, 0x00, 0x00, 0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}
