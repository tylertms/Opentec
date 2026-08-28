#include "usb/control_request.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_DIRECTION_IN = 0x80,
    USB_TYPE_STANDARD = 0x00,
    USB_TYPE_CLASS = 0x20,
    USB_TYPE_VENDOR = 0x40,
    USB_RECIPIENT_DEVICE = 0x00,
    USB_RECIPIENT_INTERFACE = 0x01,
    USB_REQUEST_GET_STATUS = 0,
    USB_REQUEST_SET_ADDRESS = 5,
    USB_REQUEST_GET_DESCRIPTOR = 6,
    USB_REQUEST_GET_CONFIGURATION = 8,
    USB_REQUEST_SET_CONFIGURATION = 9,
    USB_REQUEST_GET_INTERFACE = 10,
    USB_REQUEST_SET_INTERFACE = 11,
    USB_HID_GET_REPORT = 1,
    USB_HID_GET_IDLE = 2,
    USB_HID_GET_PROTOCOL = 3,
    USB_HID_SET_REPORT = 9,
    USB_HID_SET_IDLE = 10,
    USB_HID_SET_PROTOCOL = 11,
    USB_CDC_SET_LINE_CODING = 0x20,
    USB_CDC_GET_LINE_CODING = 0x21,
    USB_CDC_SET_CONTROL_LINE_STATE = 0x22,
    USB_XBOX_SECURITY_REQUEST = 0x90,
};

static uint16_t read_u16(const uint8_t *data) { return (uint16_t)data[0] | (uint16_t)data[1] << 8; }

bool usb_setup_packet_decode(const uint8_t data[USB_SETUP_PACKET_SIZE], UsbSetupPacket *packet) {
    if (data == 0 || packet == 0) {
        return false;
    }
    packet->request_type = data[0];
    packet->request = data[1];
    packet->value = read_u16(&data[2]);
    packet->index = read_u16(&data[4]);
    packet->length = read_u16(&data[6]);
    return true;
}

static bool set_request(const UsbSetupPacket *packet, UsbControlRequest *request,
                        UsbControlRequestKind kind) {
    request->kind = kind;
    request->value = packet->value;
    request->index = packet->index;
    request->length = packet->length;
    request->descriptor_type = (uint8_t)(packet->value >> 8);
    request->descriptor_index = (uint8_t)packet->value;
    request->recipient = packet->request_type & 0x1f;
    return true;
}

static bool classify_standard(const UsbSetupPacket *packet, UsbControlRequest *request) {
    uint8_t direction = packet->request_type & USB_DIRECTION_IN;
    uint8_t recipient = packet->request_type & 0x1f;
    switch (packet->request) {
    case USB_REQUEST_GET_STATUS:
        if (direction != 0 && packet->value == 0 && packet->length == 2) {
            return set_request(packet, request, USB_CONTROL_GET_STATUS);
        }
        break;
    case USB_REQUEST_SET_ADDRESS:
        if (packet->request_type == USB_RECIPIENT_DEVICE && packet->value <= 127 &&
            packet->index == 0 && packet->length == 0) {
            return set_request(packet, request, USB_CONTROL_SET_ADDRESS);
        }
        break;
    case USB_REQUEST_GET_DESCRIPTOR:
        if (direction != 0 && packet->length != 0 &&
            (recipient == USB_RECIPIENT_DEVICE || recipient == USB_RECIPIENT_INTERFACE)) {
            return set_request(packet, request, USB_CONTROL_GET_DESCRIPTOR);
        }
        break;
    case USB_REQUEST_GET_CONFIGURATION:
        if (packet->request_type == (USB_DIRECTION_IN | USB_RECIPIENT_DEVICE) &&
            packet->value == 0 && packet->index == 0 && packet->length == 1) {
            return set_request(packet, request, USB_CONTROL_GET_CONFIGURATION);
        }
        break;
    case USB_REQUEST_SET_CONFIGURATION:
        if (packet->request_type == USB_RECIPIENT_DEVICE && packet->value <= 1 &&
            packet->index == 0 && packet->length == 0) {
            return set_request(packet, request, USB_CONTROL_SET_CONFIGURATION);
        }
        break;
    case USB_REQUEST_GET_INTERFACE:
        if (packet->request_type == (USB_DIRECTION_IN | USB_RECIPIENT_INTERFACE) &&
            packet->value == 0 && packet->index == 0 && packet->length == 1) {
            return set_request(packet, request, USB_CONTROL_GET_INTERFACE);
        }
        break;
    case USB_REQUEST_SET_INTERFACE:
        if (packet->request_type == USB_RECIPIENT_INTERFACE && packet->index == 0 &&
            packet->length == 0) {
            return set_request(packet, request, USB_CONTROL_SET_INTERFACE);
        }
        break;
    default:
        break;
    }
    return false;
}

static bool classify_hid(const UsbSetupPacket *packet, UsbControlRequest *request) {
    if ((packet->request_type & 0x1f) != USB_RECIPIENT_INTERFACE || packet->index != 0) {
        return false;
    }
    bool input = (packet->request_type & USB_DIRECTION_IN) != 0;
    switch (packet->request) {
    case USB_HID_GET_REPORT:
        if (input && packet->length != 0) {
            return set_request(packet, request, USB_CONTROL_HID_GET_REPORT);
        }
        break;
    case USB_HID_SET_REPORT:
        if (!input && packet->length != 0) {
            return set_request(packet, request, USB_CONTROL_HID_SET_REPORT);
        }
        break;
    case USB_HID_GET_IDLE:
        if (input && packet->length == 1) {
            return set_request(packet, request, USB_CONTROL_HID_GET_IDLE);
        }
        break;
    case USB_HID_SET_IDLE:
        if (!input && packet->length == 0) {
            return set_request(packet, request, USB_CONTROL_HID_SET_IDLE);
        }
        break;
    case USB_HID_GET_PROTOCOL:
        if (input && packet->value == 0 && packet->length == 1) {
            return set_request(packet, request, USB_CONTROL_HID_GET_PROTOCOL);
        }
        break;
    case USB_HID_SET_PROTOCOL:
        if (!input && packet->value <= 1 && packet->length == 0) {
            return set_request(packet, request, USB_CONTROL_HID_SET_PROTOCOL);
        }
        break;
    default:
        break;
    }
    return false;
}

/**
 * @brief Classifies motor updater CDC interface requests.
 *
 * Accepts the seven-byte line-coding input and output requests and the zero-length control-line
 * state request for interface zero.
 *
 * @param[in] packet Decoded USB setup packet.
 * @param[out] request Classified control request.
 * @return True when the packet is a supported updater CDC request; otherwise false.
 */
static bool classify_cdc(const UsbSetupPacket *packet, UsbControlRequest *request) {
    if ((packet->request_type & 0x1f) != USB_RECIPIENT_INTERFACE || packet->index != 0 ||
        (packet->value != 0 && packet->request != USB_CDC_SET_CONTROL_LINE_STATE)) {
        return false;
    }
    bool input = (packet->request_type & USB_DIRECTION_IN) != 0;
    switch (packet->request) {
    case USB_CDC_SET_LINE_CODING:
        return !input && packet->length == 7 &&
               set_request(packet, request, USB_CONTROL_CDC_SET_LINE_CODING);
    case USB_CDC_GET_LINE_CODING:
        return input && packet->length == 7 &&
               set_request(packet, request, USB_CONTROL_CDC_GET_LINE_CODING);
    case USB_CDC_SET_CONTROL_LINE_STATE:
        return !input && packet->length == 0 &&
               set_request(packet, request, USB_CONTROL_CDC_SET_CONTROL_LINE_STATE);
    default:
        return false;
    }
}

/**
 * @brief Classifies the Xbox GIP security descriptor request.
 *
 * Accepts vendor input request 0x90 for device index 4 and retains the host-requested response
 * length.
 *
 * @param[in] packet Decoded USB setup packet.
 * @param[out] request Classified control request.
 * @return True for the Xbox GIP security descriptor request; otherwise false.
 */
static bool classify_vendor(const UsbSetupPacket *packet, UsbControlRequest *request) {
    return packet->request_type == (USB_DIRECTION_IN | USB_TYPE_VENDOR | USB_RECIPIENT_DEVICE) &&
           packet->request == USB_XBOX_SECURITY_REQUEST && packet->index == 4 &&
           set_request(packet, request, USB_CONTROL_XBOX_SECURITY_DESCRIPTOR);
}

bool usb_control_request_classify(const UsbSetupPacket *packet, UsbControlRequest *request) {
    if (packet == 0 || request == 0) {
        return false;
    }
    request->kind = USB_CONTROL_UNSUPPORTED;
    uint8_t type = packet->request_type & 0x60;
    if (type == USB_TYPE_STANDARD) {
        return classify_standard(packet, request);
    }
    if (type == USB_TYPE_CLASS) {
        return classify_cdc(packet, request) || classify_hid(packet, request);
    }
    if (type == USB_TYPE_VENDOR) {
        return classify_vendor(packet, request);
    }
    return false;
}
