#include "usb/console_descriptor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

UsbDeviceIdentity usb_playstation_device_identity(BoardVariant variant) {
    return usb_playstation_device_identity_for_mode(variant, 4);
}

UsbDeviceIdentity usb_playstation_device_identity_for_mode(BoardVariant variant,
                                                           uint8_t wheel_mode) {
    return (UsbDeviceIdentity){
        .usb_version = 0x0200,
        .vendor_id = 0x0eb7,
        .product_id = wheel_mode == 5 ? 0x0e04 : (variant == BOARD_VARIANT_DD1 ? 0x0e05 : 0x0e06),
        .device_version = 0x0059,
        .control_packet_size = 64,
        .manufacturer_string = 1,
        .product_string = 9,
    };
}

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

bool usb_xbox_gip_product_id(BoardVariant variant, uint8_t wheel_mode, uint16_t *product_id) {
    uint8_t code = usb_xbox_gip_mode_code(variant, wheel_mode);
    if (code == 0) {
        return false;
    }

    *product_id = 0x0f00u | code;
    return true;
}

const char *usb_xbox_gip_product_name(BoardVariant variant) {
    return variant == BOARD_VARIANT_DD1 ? "FANATEC Podium Wheel Base DD1"
                                        : "FANATEC Podium Wheel Base DD2";
}

const char *usb_playstation_product_name(BoardVariant variant) {
    return variant == BOARD_VARIANT_DD1 ? "FANATEC Podium Wheel Base DD1 PlayStation 4"
                                        : "FANATEC Podium Wheel Base DD2 PlayStation 4";
}

const char *usb_xbox_gip_initial_serial(void) { return "0000000000000000"; }

void usb_xbox_gip_serial_encode(const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                char output[USB_XBOX_GIP_SERIAL_SIZE]) {
    /** @brief Uppercase hexadecimal digits used for Xbox serial encoding. */
    static const char digits[] = "0123456789ABCDEF";
    for (size_t index = 0; index < USB_XBOX_GIP_DIGEST_SIZE; index++) {
        uint8_t value = digest[USB_XBOX_GIP_DIGEST_SIZE - index - 1];
        output[index * 2] = digits[value >> 4];
        output[index * 2 + 1] = digits[value & 0x0f];
    }
    output[USB_XBOX_GIP_SERIAL_TEXT_SIZE] = '\0';
}

void usb_xbox_gip_configuration_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE]) {
    /** @brief Xbox GIP configuration descriptor bytes. */
    static const uint8_t descriptor[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xe0, 0x28, 0x09, 0x04,
        0x00, 0x00, 0x02, 0xff, 0x47, 0xd0, 0x00, 0x07, 0x05, 0x01, 0x03,
        0x40, 0x00, 0x04, 0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x04,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

void usb_xbox_gip_security_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE]) {
    /** @brief Xbox GIP security descriptor bytes. */
    static const uint8_t descriptor[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE] = {
        0x28, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x58, 0x47, 0x49, 0x50, 0x31, 0x30, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

void usb_xbox_gip_os_string_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE]) {
    /** @brief Xbox Microsoft OS string descriptor bytes. */
    static const uint8_t descriptor[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE] = {
        0x12, 0x03, 0x4d, 0x00, 0x53, 0x00, 0x46, 0x00, 0x54,
        0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00, 0x90, 0x00,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

void usb_playstation_configuration_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE]) {
    /** @brief PlayStation configuration descriptor bytes. */
    static const uint8_t descriptor[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0xc0, 0x28, 0x09, 0x04, 0x00, 0x00, 0x02,
        0x03, 0x00, 0x00, 0x00, 0x09, 0x21, 0x11, 0x01, 0x21, 0x01, 0x22, 0xa0, 0x00, 0x07,
        0x05, 0x03, 0x03, 0x40, 0x00, 0x05, 0x07, 0x05, 0x84, 0x03, 0x40, 0x00, 0x05,
    };
    for (size_t index = 0; index < sizeof(descriptor); index++) {
        output[index] = descriptor[index];
    }
}

void usb_playstation_report_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE]) {
    /** @brief PlayStation HID report descriptor bytes. */
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
