#ifndef OPENTEC_BASE_USB_CONTROL_REQUEST_H
#define OPENTEC_BASE_USB_CONTROL_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

/** @brief USB setup packet size in bytes. */
enum { USB_SETUP_PACKET_SIZE = 8 /**< USB setup packet size in bytes. */ };

/** @brief Logical fields decoded from one USB setup packet. */
typedef struct {
    uint8_t request_type; /**< Direction, type, and recipient bit fields. */
    uint8_t request;      /**< USB request operation code. */
    uint16_t value;       /**< Request-specific value. */
    uint16_t index;       /**< Request-specific index. */
    uint16_t length;      /**< Number of bytes in the data stage. */
} UsbSetupPacket;

/** @brief Supported endpoint-zero request classifications. */
typedef enum {
    USB_CONTROL_UNSUPPORTED,                   /**< Setup packet is not supported. */
    USB_CONTROL_GET_STATUS,                    /**< Standard GET_STATUS request. */
    USB_CONTROL_CLEAR_FEATURE,                 /**< Standard CLEAR_FEATURE request. */
    USB_CONTROL_SET_FEATURE,                   /**< Standard SET_FEATURE request. */
    USB_CONTROL_SET_ADDRESS,                   /**< Standard SET_ADDRESS request. */
    USB_CONTROL_GET_DESCRIPTOR,                /**< Standard GET_DESCRIPTOR request. */
    USB_CONTROL_GET_CONFIGURATION,             /**< Standard GET_CONFIGURATION request. */
    USB_CONTROL_SET_CONFIGURATION,             /**< Standard SET_CONFIGURATION request. */
    USB_CONTROL_GET_INTERFACE,                 /**< Standard GET_INTERFACE request. */
    USB_CONTROL_SET_INTERFACE,                 /**< Standard SET_INTERFACE request. */
    USB_CONTROL_HID_GET_REPORT,                /**< HID GET_REPORT request. */
    USB_CONTROL_HID_SET_REPORT,                /**< HID SET_REPORT request. */
    USB_CONTROL_HID_GET_IDLE,                  /**< HID GET_IDLE request. */
    USB_CONTROL_HID_SET_IDLE,                  /**< HID SET_IDLE request. */
    USB_CONTROL_HID_GET_PROTOCOL,              /**< HID GET_PROTOCOL request. */
    USB_CONTROL_HID_SET_PROTOCOL,              /**< HID SET_PROTOCOL request. */
    USB_CONTROL_CDC_SET_LINE_CODING,           /**< CDC SET_LINE_CODING request. */
    USB_CONTROL_CDC_GET_LINE_CODING,           /**< CDC GET_LINE_CODING request. */
    USB_CONTROL_CDC_SET_CONTROL_LINE_STATE,    /**< CDC control-line-state request. */
    USB_CONTROL_CDC_SEND_ENCAPSULATED_COMMAND, /**< CDC SEND_ENCAPSULATED_COMMAND request. */
    USB_CONTROL_CDC_GET_ENCAPSULATED_RESPONSE, /**< CDC GET_ENCAPSULATED_RESPONSE request. */
    USB_CONTROL_XBOX_SECURITY_DESCRIPTOR,      /**< Xbox GIP security-descriptor request. */
} UsbControlRequestKind;

/** @brief Classified endpoint-zero request fields used by the USB control service. */
typedef struct {
    UsbControlRequestKind kind; /**< Request classification. */
    uint8_t request_type;       /**< Direction, type, and recipient bit fields. */
    uint16_t value;             /**< Request-specific value. */
    uint16_t index;             /**< Request-specific index. */
    uint16_t length;            /**< Number of bytes requested in the data stage. */
    uint8_t descriptor_type;    /**< Descriptor type extracted from value. */
    uint8_t descriptor_index;   /**< Descriptor index extracted from value. */
    uint8_t recipient;          /**< USB recipient field. */
} UsbControlRequest;

/**
 * @brief Decodes an eight-byte USB setup packet.
 *
 * Expands the request type, request, value, index, and length fields into logical fields.
 *
 * @param[in] data Eight-byte setup packet in transfer order.
 * @param[out] packet Decoded setup packet.
 * @return True when both pointers are valid and the packet was decoded; otherwise false.
 */
bool usb_setup_packet_decode(const uint8_t data[USB_SETUP_PACKET_SIZE], UsbSetupPacket *packet);

/**
 * @brief Classifies a decoded endpoint-zero request.
 *
 * Routes standard, class, and vendor request types through the supported wheel-base request sets.
 *
 * @param[in] packet Decoded USB setup packet.
 * @param[out] request Classified control request or the unsupported marker.
 * @return True when the packet is a supported request and was classified; otherwise false.
 */
bool usb_control_request_classify(const UsbSetupPacket *packet, UsbControlRequest *request);

#endif
