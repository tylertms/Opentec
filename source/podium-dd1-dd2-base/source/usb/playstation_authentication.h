#ifndef OPENTEC_BASE_USB_PLAYSTATION_AUTHENTICATION_H
#define OPENTEC_BASE_USB_PLAYSTATION_AUTHENTICATION_H

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE = 64,
    USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE = 0x100,
    USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE = 0x410,
    USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE = 16,
    USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE = 8,
};

/** @brief Status values returned in PlayStation authentication report F2. */
typedef enum {
    USB_PLAYSTATION_AUTHENTICATION_IDLE = 0,
    USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ACTIVE = 1,
    USB_PLAYSTATION_AUTHENTICATION_PENDING = 0x10,
    USB_PLAYSTATION_AUTHENTICATION_CHECKSUM_ERROR = 0xf0,
    USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ERROR = 0xf2,
} UsbPlaystationAuthenticationStatus;

/** @brief State for assembling requests and exposing a retained authentication response. */
typedef struct {
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE];
    const uint8_t *response;
    uint8_t sequence;
    uint8_t response_index;
    uint8_t receive_chunk_size;
    uint8_t transmit_chunk_size;
    UsbPlaystationAuthenticationStatus status;
    bool checksum_enabled;
    bool request_ready;
    bool response_ready;
} UsbPlaystationAuthentication;

void usb_playstation_authentication_init(UsbPlaystationAuthentication *authentication);
void usb_playstation_authentication_format_report(
    UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE]);
bool usb_playstation_authentication_receive(
    UsbPlaystationAuthentication *authentication,
    const uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE]);
bool usb_playstation_authentication_take_request(
    UsbPlaystationAuthentication *authentication,
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]);
bool usb_playstation_authentication_publish_response(UsbPlaystationAuthentication *authentication,
                                                     const uint8_t *response,
                                                     uint16_t response_length);
void usb_playstation_authentication_fail(UsbPlaystationAuthentication *authentication);
void usb_playstation_authentication_status_report(
    const UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE]);
bool usb_playstation_authentication_response_report(
    UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE]);
bool usb_playstation_authentication_response_active(
    const UsbPlaystationAuthentication *authentication);

#endif
