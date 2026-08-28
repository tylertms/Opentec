#ifndef OPENTEC_BASE_USB_CONTROL_REQUEST_H
#define OPENTEC_BASE_USB_CONTROL_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

enum { USB_SETUP_PACKET_SIZE = 8 };

typedef struct {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} UsbSetupPacket;

typedef enum {
    USB_CONTROL_UNSUPPORTED,
    USB_CONTROL_GET_STATUS,
    USB_CONTROL_SET_ADDRESS,
    USB_CONTROL_GET_DESCRIPTOR,
    USB_CONTROL_GET_CONFIGURATION,
    USB_CONTROL_SET_CONFIGURATION,
    USB_CONTROL_GET_INTERFACE,
    USB_CONTROL_SET_INTERFACE,
    USB_CONTROL_HID_GET_REPORT,
    USB_CONTROL_HID_SET_REPORT,
    USB_CONTROL_HID_GET_IDLE,
    USB_CONTROL_HID_SET_IDLE,
    USB_CONTROL_HID_GET_PROTOCOL,
    USB_CONTROL_HID_SET_PROTOCOL,
    USB_CONTROL_CDC_SET_LINE_CODING,
    USB_CONTROL_CDC_GET_LINE_CODING,
    USB_CONTROL_CDC_SET_CONTROL_LINE_STATE,
} UsbControlRequestKind;

typedef struct {
    UsbControlRequestKind kind;
    uint16_t value;
    uint16_t index;
    uint16_t length;
    uint8_t descriptor_type;
    uint8_t descriptor_index;
    uint8_t recipient;
} UsbControlRequest;

bool usb_setup_packet_decode(const uint8_t data[USB_SETUP_PACKET_SIZE], UsbSetupPacket *packet);
bool usb_control_request_classify(const UsbSetupPacket *packet, UsbControlRequest *request);

#endif
