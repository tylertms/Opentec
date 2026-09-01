#ifndef OPENTEC_BASE_USB_CONSOLE_DESCRIPTOR_H
#define OPENTEC_BASE_USB_CONSOLE_DESCRIPTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/descriptor.h"

/** @brief Sizes of console identity, string, and descriptor buffers. */
enum {
    USB_XBOX_GIP_DIGEST_SIZE = 8, /**< Attached-wheel digest size in bytes. */
    USB_XBOX_GIP_SERIAL_TEXT_SIZE = USB_XBOX_GIP_DIGEST_SIZE * 2, /**< Xbox serial text length. */
    USB_XBOX_GIP_SERIAL_SIZE = USB_XBOX_GIP_SERIAL_TEXT_SIZE + 1, /**< Xbox serial buffer size. */
    USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE = 32, /**< Xbox configuration descriptor size. */
    USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE = 40,      /**< Xbox security descriptor size. */
    USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE = 18, /**< Xbox Microsoft OS string descriptor size. */
    USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE = 41, /**< PlayStation configuration size. */
    USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE = 160, /**< PlayStation HID report descriptor size. */
};

/**
 * @brief Builds the Xbox GIP USB device identity.
 *
 * Retains the supplied product identifier while applying the common Fanatec vendor and GIP device
 * descriptor fields.
 *
 * @param[in] product_id Runtime Xbox GIP product identifier.
 * @return Xbox GIP USB identity.
 */
UsbDeviceIdentity usb_xbox_gip_device_identity(uint16_t product_id);

/**
 * @brief Builds the PlayStation USB device identity.
 *
 * Selects the DD1 or DD2 product identifier for the default PlayStation wheel mode.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @return PlayStation USB identity.
 */
UsbDeviceIdentity usb_playstation_device_identity(BoardVariant variant);

/**
 * @brief Builds a PlayStation USB identity for a selected wheel mode.
 *
 * Selects the mode-specific product identifier while retaining the common Fanatec identity fields.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @param[in] wheel_mode Selected PlayStation base mode.
 * @return Matching PlayStation USB identity.
 */
UsbDeviceIdentity usb_playstation_device_identity_for_mode(BoardVariant variant,
                                                           uint8_t wheel_mode);

/**
 * @brief Selects the Xbox GIP mode code.
 *
 * Maps supported attached-wheel modes to the DD1 or DD2 code family used by Xbox discovery.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @param[in] wheel_mode Attached-wheel operating-mode selector.
 * @return DD1 or DD2 mode code, or zero for an unsupported wheel mode.
 */
uint8_t usb_xbox_gip_mode_code(BoardVariant variant, uint8_t wheel_mode);

/**
 * @brief Selects an Xbox GIP product identifier.
 *
 * Maps a supported attached-wheel mode to its DD1 or DD2 product identifier.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @param[in] wheel_mode Attached-wheel operating-mode selector.
 * @param[out] product_id Destination for the selected product identifier.
 * @return True when the wheel mode has a product identifier; otherwise false.
 */
bool usb_xbox_gip_product_id(BoardVariant variant, uint8_t wheel_mode, uint16_t *product_id);

/**
 * @brief Returns the Xbox GIP product name.
 *
 * Supplies the DD1 or DD2 Podium Wheel Base name used by the Xbox interface.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @return Null-terminated Xbox GIP product name.
 */
const char *usb_xbox_gip_product_name(BoardVariant variant);

/**
 * @brief Returns the PlayStation product name.
 *
 * Supplies the DD1 or DD2 Podium Wheel Base name used by the PlayStation interface.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @return Null-terminated PlayStation product name.
 */
const char *usb_playstation_product_name(BoardVariant variant);

/**
 * @brief Returns the initial Xbox GIP serial text.
 *
 * Supplies the sixteen-character placeholder used before the wheel-status digest is rendered.
 *
 * @return Null-terminated initial Xbox GIP serial text.
 */
const char *usb_xbox_gip_initial_serial(void);

/**
 * @brief Encodes Xbox GIP serial text from an attached-wheel digest.
 *
 * Renders the digest in reverse byte order with uppercase hexadecimal digits.
 *
 * @param[in] digest Eight-byte attached-wheel status digest.
 * @param[out] output Sixteen-character serial text and terminating null byte.
 */
void usb_xbox_gip_serial_encode(const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                char output[USB_XBOX_GIP_SERIAL_SIZE]);

/**
 * @brief Encodes the Xbox GIP USB configuration descriptor.
 *
 * Emits the vendor-specific interface with its two 64-byte interrupt endpoints.
 *
 * @param[out] output Destination for the 32-byte configuration descriptor.
 */
void usb_xbox_gip_configuration_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE]);

/**
 * @brief Encodes the Xbox GIP security descriptor.
 *
 * Emits the vendor response selected by the Xbox security-descriptor request.
 *
 * @param[out] output Destination for the 40-byte security descriptor.
 */
void usb_xbox_gip_security_descriptor_encode(uint8_t output[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE]);

/**
 * @brief Encodes the Xbox GIP Microsoft OS string descriptor.
 *
 * Emits the MSFT100 signature and vendor request code used by Xbox enumeration.
 *
 * @param[out] output Destination for the 18-byte string descriptor.
 */
void usb_xbox_gip_os_string_descriptor_encode(
    uint8_t output[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE]);

/**
 * @brief Encodes the PlayStation USB configuration descriptor.
 *
 * Emits the HID interface and its 64-byte interrupt input and output endpoints.
 *
 * @param[out] output Destination for the 41-byte configuration descriptor.
 */
void usb_playstation_configuration_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE]);

/**
 * @brief Encodes the PlayStation HID report descriptor.
 *
 * Emits the gamepad input, output, and feature report layouts used by the PlayStation interface.
 *
 * @param[out] output Destination for the 160-byte HID report descriptor.
 */
void usb_playstation_report_descriptor_encode(
    uint8_t output[USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE]);

#endif
