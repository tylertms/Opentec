#ifndef OPENTEC_BASE_USB_DESCRIPTOR_H
#define OPENTEC_BASE_USB_DESCRIPTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    USB_DEVICE_DESCRIPTOR_SIZE = 18,
    USB_HID_CONFIGURATION_DESCRIPTOR_SIZE = 41,
};

typedef struct {
    uint16_t usb_version;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t control_packet_size;
    uint8_t manufacturer_string;
    uint8_t product_string;
    uint8_t serial_string;
} UsbDeviceIdentity;

typedef struct {
    uint16_t hid_version;
    uint16_t report_descriptor_size;
    uint16_t endpoint_packet_size;
    uint16_t maximum_power_ma;
    uint8_t country_code;
    uint8_t input_endpoint;
    uint8_t output_endpoint;
    uint8_t poll_interval_ms;
    uint8_t interface_protocol;
    uint8_t interface_string;
    bool self_powered;
    bool remote_wakeup;
} UsbHidConfiguration;

void usb_device_descriptor_encode(const UsbDeviceIdentity *identity,
                                  uint8_t output[USB_DEVICE_DESCRIPTOR_SIZE]);
void usb_hid_configuration_descriptor_encode(const UsbHidConfiguration *configuration,
                                             uint8_t output[USB_HID_CONFIGURATION_DESCRIPTOR_SIZE]);
size_t usb_language_descriptor_encode(uint16_t language_id, uint8_t *output, size_t capacity);
size_t usb_string_descriptor_encode(const char *text, uint8_t *output, size_t capacity);

#endif
