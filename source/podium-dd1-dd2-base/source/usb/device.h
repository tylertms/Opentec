#ifndef OPENTEC_BASE_USB_DEVICE_H
#define OPENTEC_BASE_USB_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/console_descriptor.h"
#include "usb/input_report.h"
#include "usb/playstation_authentication.h"
#include "usb/playstation_input.h"
#include "usb/xbox_gip_response.h"
#include "usb/xbox_gip_session.h"

enum {
    USB_DEVICE_REPORT_SIZE = 64,
    USB_DEVICE_UPDATER_RESPONSE_SIZE = 66,
    USB_DEVICE_HID_REPORT_INPUT = 1,
    USB_DEVICE_HID_REPORT_OUTPUT = 2,
    USB_DEVICE_HID_REPORT_FEATURE = 3,
};

typedef enum {
    USB_OPERATING_MODE_FANATEC = 0,
    USB_OPERATING_MODE_FANATEC_COMPATIBILITY = 1,
    USB_OPERATING_MODE_DRIVING_FORCE_EX = 2,
    USB_OPERATING_MODE_DRIVING_FORCE_PRO = 3,
    USB_OPERATING_MODE_G27 = 4,
    USB_OPERATING_MODE_UPDATER = 5,
    USB_OPERATING_MODE_XBOX_GIP = 6,
    USB_OPERATING_MODE_PLAYSTATION = 7,
} UsbOperatingMode;

typedef struct {
    uint8_t report_type;
    uint8_t report_id;
    uint8_t length;
    uint8_t data[USB_DEVICE_REPORT_SIZE];
} UsbDeviceOutputReport;

typedef struct {
    uint8_t length;
    uint8_t data[USB_DEVICE_REPORT_SIZE];
} UsbDeviceUpdaterPacket;

void usb_device_init(BoardVariant variant);
void usb_device_prepare(BoardVariant variant);
bool usb_device_set_input_mode(UsbInputReportMode mode);
UsbInputReportMode usb_device_input_mode(void);
bool usb_device_set_operating_mode(UsbOperatingMode mode);
bool usb_device_set_xbox_mode(uint8_t wheel_mode, const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE]);
bool usb_device_set_playstation_mode(void);
bool usb_device_set_playstation_wheel_mode(uint8_t wheel_mode);
UsbOperatingMode usb_device_operating_mode(void);
void usb_device_service(void);
bool usb_device_configured(void);
bool usb_device_take_output(UsbDeviceOutputReport *report);
bool usb_device_publish_feature_report(uint8_t report_id, const uint8_t *report, uint8_t length);
bool usb_device_take_feature_report_request(uint8_t report_id);
bool usb_device_send_input(const uint8_t *report, uint8_t length);
bool usb_device_queue_xbox_input(const UsbXboxGipInputSnapshot *snapshot);
bool usb_device_queue_xbox_capabilities(void);
bool usb_device_queue_xbox_extended_status(const UsbXboxGipExtendedStatus *status);
bool usb_device_queue_xbox_transfer_status(const uint8_t request[2]);
bool usb_device_queue_xbox_response(const uint8_t *report, uint8_t length);
bool usb_device_queue_xbox_vendor_report(const uint8_t report[USB_DEVICE_REPORT_SIZE]);
bool usb_device_send_vendor_report(const uint8_t *report, uint8_t length);
bool usb_device_take_updater_packet(UsbDeviceUpdaterPacket *packet);
bool usb_device_updater_channel_idle(void);
bool usb_device_queue_updater_response(const uint8_t *data, uint8_t length);
UsbXboxGipSessionAction usb_device_take_xbox_session_actions(void);
bool usb_device_take_playstation_authentication_request(
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]);
bool usb_device_publish_playstation_authentication_response(const uint8_t *response,
                                                            uint16_t response_length);
bool usb_device_playstation_authentication_response_active(void);
void usb_device_fail_playstation_authentication(void);
bool usb_device_publish_playstation_remote_tuning_report(
    const uint8_t report[USB_DEVICE_REPORT_SIZE]);
bool usb_device_send_playstation_input(const UsbPlaystationInputState *state);

#endif
