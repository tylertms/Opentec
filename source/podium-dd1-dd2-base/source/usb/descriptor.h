#ifndef OPENTEC_BASE_USB_DESCRIPTOR_H
#define OPENTEC_BASE_USB_DESCRIPTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Sizes of the USB device and primary HID configuration descriptors. */
enum {
    USB_DEVICE_DESCRIPTOR_SIZE = 18,            /**< USB device descriptor size in bytes. */
    USB_HID_CONFIGURATION_DESCRIPTOR_SIZE = 41, /**< Primary HID configuration size in bytes. */
};

/** @brief USB device identity and enumeration fields. */
typedef struct {
    uint16_t usb_version;        /**< USB specification version in binary-coded decimal. */
    uint16_t vendor_id;          /**< USB vendor identifier. */
    uint16_t product_id;         /**< USB product identifier. */
    uint16_t device_version;     /**< Device release number in binary-coded decimal. */
    uint8_t device_class;        /**< USB device class code. */
    uint8_t device_subclass;     /**< USB device subclass code. */
    uint8_t device_protocol;     /**< USB device protocol code. */
    uint8_t control_packet_size; /**< Endpoint-zero maximum packet size. */
    uint8_t manufacturer_string; /**< Manufacturer string descriptor index. */
    uint8_t product_string;      /**< Product string descriptor index. */
    uint8_t serial_string;       /**< Serial-number string descriptor index. */
} UsbDeviceIdentity;

/** @brief Primary HID interface, report, endpoint, and power configuration. */
typedef struct {
    uint16_t hid_version;            /**< HID class specification version. */
    uint16_t report_descriptor_size; /**< HID report descriptor size in bytes. */
    uint16_t endpoint_packet_size;   /**< Interrupt endpoint maximum packet size. */
    uint16_t maximum_power_ma;       /**< Maximum bus power in milliamperes. */
    uint8_t country_code;            /**< HID country code. */
    uint8_t input_endpoint;          /**< Device-to-host interrupt endpoint address. */
    uint8_t output_endpoint;         /**< Host-to-device interrupt endpoint address. */
    uint8_t poll_interval_ms;        /**< Interrupt endpoint polling interval in milliseconds. */
    uint8_t interface_protocol;      /**< HID interface protocol code. */
    uint8_t interface_string;        /**< HID interface string descriptor index. */
    bool self_powered;               /**< True when the configuration is self-powered. */
    bool remote_wakeup;              /**< True when the configuration supports remote wakeup. */
} UsbHidConfiguration;

/**
 * @brief Encodes an eighteen-byte USB device descriptor.
 *
 * Emits the supplied identity, endpoint-zero packet size, string indices, and one supported
 * configuration in USB descriptor order.
 *
 * @param[in] identity Device identity and enumeration fields.
 * @param[out] output Complete device descriptor.
 */
void usb_device_descriptor_encode(const UsbDeviceIdentity *identity,
                                  uint8_t output[USB_DEVICE_DESCRIPTOR_SIZE]);

/**
 * @brief Encodes the primary HID configuration descriptor.
 *
 * Concatenates the configuration, interface, HID, input-endpoint, and output-endpoint descriptors
 * in the enumerated 41-byte order.
 *
 * @param[in] configuration HID interface, endpoint, and power configuration.
 * @param[out] output Complete HID configuration descriptor.
 */
void usb_hid_configuration_descriptor_encode(const UsbHidConfiguration *configuration,
                                             uint8_t output[USB_HID_CONFIGURATION_DESCRIPTOR_SIZE]);

/**
 * @brief Encodes the supported-language string descriptor.
 *
 * Emits one four-byte descriptor containing the requested USB language identifier.
 *
 * @param[in] language_id USB language identifier.
 * @param[out] output Destination byte buffer.
 * @param[in] capacity Available destination bytes.
 * @return Four on success, or zero when capacity is less than four bytes.
 */
size_t usb_language_descriptor_encode(uint16_t language_id, uint8_t *output, size_t capacity);

/**
 * @brief Encodes an ASCII USB string descriptor.
 *
 * Expands each source byte into the low byte of one UTF-16LE code unit and rejects text that cannot
 * fit the one-byte descriptor length or supplied destination capacity.
 *
 * @param[in] text Null-terminated ASCII text.
 * @param[out] output Destination byte buffer.
 * @param[in] capacity Available destination bytes.
 * @return Encoded descriptor length, or zero when the text is longer than 126 bytes or capacity is
 * insufficient.
 */
size_t usb_string_descriptor_encode(const char *text, uint8_t *output, size_t capacity);

#endif
