#include "usb/descriptor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    USB_DESCRIPTOR_DEVICE = 1,
    USB_DESCRIPTOR_CONFIGURATION = 2,
    USB_DESCRIPTOR_STRING = 3,
    USB_DESCRIPTOR_INTERFACE = 4,
    USB_DESCRIPTOR_ENDPOINT = 5,
    USB_DESCRIPTOR_HID = 0x21,
    USB_DESCRIPTOR_HID_REPORT = 0x22,
    USB_CLASS_HID = 3,
    USB_ENDPOINT_INTERRUPT = 3,
};

/**
 * @brief Writes a little-endian sixteen-bit descriptor field.
 *
 * Stores the low byte followed by the high byte.
 *
 * @param[out] output Two-byte destination field.
 * @param[in] value Field value to encode.
 */
static void write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Encodes an eighteen-byte USB device descriptor.
 *
 * Emits the USB version, class tuple, endpoint-zero packet size, device identifiers, string
 * indices, and the single supported configuration in descriptor order.
 *
 * @param[in] identity Device identity and enumeration fields.
 * @param[out] output Complete device descriptor.
 */
void usb_device_descriptor_encode(const UsbDeviceIdentity *identity,
                                  uint8_t output[USB_DEVICE_DESCRIPTOR_SIZE]) {
    output[0] = USB_DEVICE_DESCRIPTOR_SIZE;
    output[1] = USB_DESCRIPTOR_DEVICE;
    write_u16(&output[2], identity->usb_version);
    output[4] = identity->device_class;
    output[5] = identity->device_subclass;
    output[6] = identity->device_protocol;
    output[7] = identity->control_packet_size;
    write_u16(&output[8], identity->vendor_id);
    write_u16(&output[10], identity->product_id);
    write_u16(&output[12], identity->device_version);
    output[14] = identity->manufacturer_string;
    output[15] = identity->product_string;
    output[16] = identity->serial_string;
    output[17] = 1;
}

/**
 * @brief Encodes the configuration header for the primary HID interface.
 *
 * Emits one interface, the fixed 41-byte descriptor length, configuration value one, power
 * attributes, and maximum power in two-milliampere units.
 *
 * @param[in] configuration HID interface and power configuration.
 * @param[out] output Nine-byte configuration header.
 */
static void encode_configuration(const UsbHidConfiguration *configuration, uint8_t *output) {
    output[0] = 9;
    output[1] = USB_DESCRIPTOR_CONFIGURATION;
    write_u16(&output[2], USB_HID_CONFIGURATION_DESCRIPTOR_SIZE);
    output[4] = 1;
    output[5] = 1;
    output[6] = 0;
    output[7] =
        0x80 | (configuration->self_powered ? 0x40 : 0) | (configuration->remote_wakeup ? 0x20 : 0);
    uint16_t power_units = (configuration->maximum_power_ma + 1) / 2;
    output[8] = power_units > UINT8_MAX ? UINT8_MAX : (uint8_t)power_units;
}

/**
 * @brief Encodes the primary HID interface descriptor.
 *
 * Emits interface zero with alternate setting zero, two endpoints, HID class, and the selected
 * interface protocol.
 *
 * @param[in] configuration HID interface configuration.
 * @param[out] output Nine-byte interface descriptor.
 */
static void encode_interface(const UsbHidConfiguration *configuration, uint8_t *output) {
    output[0] = 9;
    output[1] = USB_DESCRIPTOR_INTERFACE;
    output[2] = 0;
    output[3] = 0;
    output[4] = 2;
    output[5] = USB_CLASS_HID;
    output[6] = 0;
    output[7] = configuration->interface_protocol;
    output[8] = 0;
}

/**
 * @brief Encodes the HID class descriptor.
 *
 * Emits the HID version, country code, one subordinate report descriptor, and its byte length.
 *
 * @param[in] configuration HID class and report configuration.
 * @param[out] output Nine-byte HID descriptor.
 */
static void encode_hid(const UsbHidConfiguration *configuration, uint8_t *output) {
    output[0] = 9;
    output[1] = USB_DESCRIPTOR_HID;
    write_u16(&output[2], configuration->hid_version);
    output[4] = configuration->country_code;
    output[5] = 1;
    output[6] = USB_DESCRIPTOR_HID_REPORT;
    write_u16(&output[7], configuration->report_descriptor_size);
}

/**
 * @brief Encodes one interrupt endpoint descriptor.
 *
 * Emits the endpoint address, packet size, and polling interval shared by the primary HID pair.
 *
 * @param[in] address Direction-qualified endpoint address.
 * @param[in] configuration HID endpoint configuration.
 * @param[out] output Seven-byte endpoint descriptor.
 */
static void encode_endpoint(uint8_t address, const UsbHidConfiguration *configuration,
                            uint8_t *output) {
    output[0] = 7;
    output[1] = USB_DESCRIPTOR_ENDPOINT;
    output[2] = address;
    output[3] = USB_ENDPOINT_INTERRUPT;
    write_u16(&output[4], configuration->endpoint_packet_size);
    output[6] = configuration->poll_interval_ms;
}

/**
 * @brief Encodes the complete primary HID configuration descriptor.
 *
 * Concatenates the configuration, interface, HID, input-endpoint, and output-endpoint descriptors
 * in the enumerated 41-byte order.
 *
 * @param[in] configuration HID interface, endpoint, and power configuration.
 * @param[out] output Complete HID configuration descriptor.
 */
void usb_hid_configuration_descriptor_encode(
    const UsbHidConfiguration *configuration,
    uint8_t output[USB_HID_CONFIGURATION_DESCRIPTOR_SIZE]) {
    encode_configuration(configuration, output);
    encode_interface(configuration, &output[9]);
    encode_hid(configuration, &output[18]);
    encode_endpoint(configuration->input_endpoint, configuration, &output[27]);
    encode_endpoint(configuration->output_endpoint, configuration, &output[34]);
}

/**
 * @brief Encodes the supported-language string descriptor.
 *
 * Emits one four-byte descriptor containing the requested USB language identifier.
 *
 * @param[in] language_id USB language identifier.
 * @param[out] output Destination byte buffer.
 * @param[in] capacity Available destination bytes.
 * @return Four on success, or zero when the destination is too small.
 */
size_t usb_language_descriptor_encode(uint16_t language_id, uint8_t *output, size_t capacity) {
    if (capacity < 4) {
        return 0;
    }
    output[0] = 4;
    output[1] = USB_DESCRIPTOR_STRING;
    write_u16(&output[2], language_id);
    return 4;
}

/**
 * @brief Encodes an ASCII USB string descriptor.
 *
 * Expands each source byte into the low byte of one UTF-16LE code unit and rejects text that
 * cannot fit the one-byte descriptor length or the supplied destination.
 *
 * @param[in] text Null-terminated ASCII text.
 * @param[out] output Destination byte buffer.
 * @param[in] capacity Available destination bytes.
 * @return Encoded descriptor length, or zero when the text or destination is unsupported.
 */
size_t usb_string_descriptor_encode(const char *text, uint8_t *output, size_t capacity) {
    size_t text_length = strlen(text);
    size_t descriptor_length = text_length * 2 + 2;
    if (text_length > 126 || capacity < descriptor_length) {
        return 0;
    }

    output[0] = (uint8_t)descriptor_length;
    output[1] = USB_DESCRIPTOR_STRING;
    for (size_t index = 0; index < text_length; index++) {
        output[index * 2 + 2] = (uint8_t)text[index];
        output[index * 2 + 3] = 0;
    }
    return descriptor_length;
}
