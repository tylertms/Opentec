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

static void write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

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

static void encode_hid(const UsbHidConfiguration *configuration, uint8_t *output) {
    output[0] = 9;
    output[1] = USB_DESCRIPTOR_HID;
    write_u16(&output[2], configuration->hid_version);
    output[4] = configuration->country_code;
    output[5] = 1;
    output[6] = USB_DESCRIPTOR_HID_REPORT;
    write_u16(&output[7], configuration->report_descriptor_size);
}

static void encode_endpoint(uint8_t address, const UsbHidConfiguration *configuration,
                            uint8_t *output) {
    output[0] = 7;
    output[1] = USB_DESCRIPTOR_ENDPOINT;
    output[2] = address;
    output[3] = USB_ENDPOINT_INTERRUPT;
    write_u16(&output[4], configuration->endpoint_packet_size);
    output[6] = configuration->poll_interval_ms;
}

void usb_hid_configuration_descriptor_encode(
    const UsbHidConfiguration *configuration,
    uint8_t output[USB_HID_CONFIGURATION_DESCRIPTOR_SIZE]) {
    encode_configuration(configuration, output);
    encode_interface(configuration, &output[9]);
    encode_hid(configuration, &output[18]);
    encode_endpoint(configuration->input_endpoint, configuration, &output[27]);
    encode_endpoint(configuration->output_endpoint, configuration, &output[34]);
}

size_t usb_language_descriptor_encode(uint16_t language_id, uint8_t *output, size_t capacity) {
    if (capacity < 4) {
        return 0;
    }
    output[0] = 4;
    output[1] = USB_DESCRIPTOR_STRING;
    write_u16(&output[2], language_id);
    return 4;
}

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
