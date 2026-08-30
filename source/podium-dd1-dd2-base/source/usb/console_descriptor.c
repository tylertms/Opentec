#include "usb/console_descriptor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Returns the Xbox GIP USB device identity.
 *
 * Describes the vendor-specific GIP device while retaining the product ID selected for the
 * attached wheel and base combination.
 *
 * @param[in] product_id Runtime Xbox GIP product identifier.
 * @return Xbox GIP USB identity.
 */
UsbDeviceIdentity usb_xbox_gip_device_identity(uint16_t product_id) {
    return (UsbDeviceIdentity){
        .usb_version = 0x0200,
        .vendor_id = 0x0eb7,
        .product_id = product_id,
        .device_version = 0x0059,
        .device_class = 0xff,
        .device_subclass = 0xff,
        .device_protocol = 0xff,
        .control_packet_size = 64,
        .manufacturer_string = 1,
        .product_string = 2,
        .serial_string = 3,
    };
}

/**
 * @brief Returns the PlayStation USB device identity.
 *
 * Selects product ID 0x0E05 for DD1 hardware and 0x0E06 for DD2 hardware while retaining the
 * common Fanatec identity and PlayStation product string index.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @return PlayStation USB identity.
 */
UsbDeviceIdentity usb_playstation_device_identity(BoardVariant variant) {
    return (UsbDeviceIdentity){
        .usb_version = 0x0200,
        .vendor_id = 0x0eb7,
        .product_id = variant == BOARD_VARIANT_DD1 ? 0x0e05 : 0x0e06,
        .device_version = 0x0059,
        .control_packet_size = 64,
        .manufacturer_string = 1,
        .product_string = 9,
    };
}

/**
 * @brief Selects the Xbox GIP mode code.
 *
 * Maps the attached wheel mode to the low byte shared by discovery responses and the USB product
 * identifier. Unsupported wheel modes return zero.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @param[in] wheel_mode Attached wheel operating-mode selector.
 * @return DD1 or DD2 mode code, or zero for an unsupported wheel mode.
 */
uint8_t usb_xbox_gip_mode_code(BoardVariant variant, uint8_t wheel_mode) {
    uint8_t code;
    switch (wheel_mode) {
    case 6:
    case 21:
        code = 0x50;
        break;
    case 7:
    case 18:
        code = 0x51;
        break;
    case 9:
    case 11:
    case 29:
        code = 0x53;
        break;
    case 10:
        code = 0x54;
        break;
    default:
        return 0;
    }

    return variant == BOARD_VARIANT_DD1 ? code : code + 0x10;
}

/**
 * @brief Selects the Xbox GIP product identifier.
 *
 * Maps the attached wheel mode to the DD1 or DD2 identifier family. Unsupported wheel modes leave
 * the supplied identifier unchanged.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @param[in] wheel_mode Attached wheel operating-mode selector.
 * @param[out] product_id Destination for the selected product identifier.
 * @return True when the wheel mode has a product identifier; otherwise false.
 */
bool usb_xbox_gip_product_id(BoardVariant variant, uint8_t wheel_mode, uint16_t *product_id) {
    uint8_t code = usb_xbox_gip_mode_code(variant, wheel_mode);
    if (code == 0) {
        return false;
    }

    *product_id = 0x0f00u | code;
    return true;
}

/**
 * @brief Returns the Xbox GIP product name.
 *
 * Supplies the DD1 or DD2 Podium Wheel Base name returned for Xbox string request index 2.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @return Null-terminated Xbox GIP product name.
 */
const char *usb_xbox_gip_product_name(BoardVariant variant) {
    return variant == BOARD_VARIANT_DD1 ? "FANATEC Podium Wheel Base DD1"
                                        : "FANATEC Podium Wheel Base DD2";
}

/**
 * @brief Returns the PlayStation product name.
 *
 * Supplies the DD1 or DD2 Podium Wheel Base name returned for PlayStation string request index 9.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @return Null-terminated PlayStation product name.
 */
const char *usb_playstation_product_name(BoardVariant variant) {
    return variant == BOARD_VARIANT_DD1 ? "FANATEC Podium Wheel Base DD1 PlayStation 4"
                                        : "FANATEC Podium Wheel Base DD2 PlayStation 4";
}

/**
 * @brief Returns the initial Xbox GIP serial text.
 *
 * Supplies the sixteen-character placeholder used before the wheel-status digest is rendered.
 *
 * @return Null-terminated initial Xbox GIP serial text.
 */
const char *usb_xbox_gip_initial_serial(void) { return "0000000000000000"; }

/**
 * @brief Encodes the Xbox GIP serial text.
 *
 * Renders the attached-wheel digest in reverse byte order with uppercase hexadecimal digits.
 *
 * @param[in] digest Eight-byte attached-wheel status digest.
 * @param[out] output Sixteen-character serial text and terminating null byte.
 */
void usb_xbox_gip_serial_encode(const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                char output[USB_XBOX_GIP_SERIAL_SIZE]) {
    static const char digits[] = "0123456789ABCDEF";
    for (size_t index = 0; index < USB_XBOX_GIP_DIGEST_SIZE; index++) {
        uint8_t value = digest[USB_XBOX_GIP_DIGEST_SIZE - index - 1];
        output[index * 2] = digits[value >> 4];
        output[index * 2 + 1] = digits[value & 0x0f];
    }
    output[USB_XBOX_GIP_SERIAL_TEXT_SIZE] = '\0';
}

/**
 * @brief Encodes the Xbox GIP USB configuration descriptor.
 *
 * Emits the vendor-specific interface with 64-byte interrupt output endpoint 0x01 and input
 * endpoint 0x81, both polled every four milliseconds.
 *
 * @param[out] output Destination for the 32-byte configuration descriptor.
 */
void usb_xbox_gip_configuration_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE]) {
    static const uint8_t descriptor[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xe0, 0x28, 0x09, 0x04,
        0x00, 0x00, 0x02, 0xff, 0x47, 0xd0, 0x00, 0x07, 0x05, 0x01, 0x03,
        0x40, 0x00, 0x04, 0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x04,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

/**
 * @brief Encodes the Xbox GIP security descriptor.
 *
 * Emits the 40-byte vendor response selected by request 0x90 with index 4, including the GIP10
 * capability signature.
 *
 * @param[out] output Destination for the 40-byte security descriptor.
 */
void usb_xbox_gip_security_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE]) {
    static const uint8_t descriptor[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE] = {
        0x28, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x58, 0x47, 0x49, 0x50, 0x31, 0x30, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

/**
 * @brief Encodes the Xbox GIP Microsoft OS string descriptor.
 *
 * Emits string request index 0xEE as the MSFT100 signature followed by vendor request code 0x90.
 *
 * @param[out] output Destination for the 18-byte string descriptor.
 */
void usb_xbox_gip_os_string_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE]) {
    static const uint8_t descriptor[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE] = {
        0x12, 0x03, 0x4d, 0x00, 0x53, 0x00, 0x46, 0x00, 0x54,
        0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00, 0x90, 0x00,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

/**
 * @brief Encodes the PlayStation USB configuration descriptor.
 *
 * Emits the HID interface with 64-byte interrupt output endpoint 0x03 and input endpoint 0x84,
 * both polled every five milliseconds.
 *
 * @param[out] output Destination for the 41-byte configuration descriptor.
 */
void usb_playstation_configuration_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE]) {
    static const uint8_t descriptor[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0xc0, 0x28, 0x09, 0x04, 0x00, 0x00, 0x02,
        0x03, 0x00, 0x00, 0x00, 0x09, 0x21, 0x11, 0x01, 0x21, 0x01, 0x22, 0xa0, 0x00, 0x07,
        0x05, 0x03, 0x03, 0x40, 0x00, 0x05, 0x07, 0x05, 0x84, 0x03, 0x40, 0x00, 0x05,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

/**
 * @brief Encodes the PlayStation HID report descriptor.
 *
 * Emits the gamepad input report, output report 5, feature report 3, and vendor feature reports
 * F0 through F3 used by the PlayStation interface.
 *
 * @param[out] output Destination for the 160-byte HID report descriptor.
 */
void usb_playstation_report_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE]) {
    static const uint8_t descriptor[USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE] = {
        0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09,
        0x35, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, 0x09, 0x39, 0x15,
        0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3b, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81,
        0x42, 0x65, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0e, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
        0x95, 0x0e, 0x81, 0x02, 0x06, 0x00, 0xff, 0x09, 0x20, 0x75, 0x06, 0x95, 0x01, 0x81, 0x02,
        0x05, 0x01, 0x09, 0x33, 0x09, 0x34, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x02,
        0x81, 0x02, 0x06, 0x00, 0xff, 0x09, 0x21, 0x95, 0x36, 0x81, 0x02, 0x85, 0x05, 0x09, 0x22,
        0x95, 0x1f, 0x91, 0x02, 0x85, 0x03, 0x0a, 0x21, 0x27, 0x95, 0x2f, 0xb1, 0x02, 0xc0, 0x06,
        0xf0, 0xff, 0x09, 0x40, 0xa1, 0x01, 0x85, 0xf0, 0x09, 0x47, 0x95, 0x3f, 0xb1, 0x02, 0x85,
        0xf1, 0x09, 0x48, 0x95, 0x3f, 0xb1, 0x02, 0x85, 0xf2, 0x09, 0x49, 0x95, 0x0f, 0xb1, 0x02,
        0x85, 0xf3, 0x0a, 0x01, 0x47, 0x95, 0x07, 0xb1, 0x02, 0xc0,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}
