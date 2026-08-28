#ifndef OPENTEC_BASE_USB_DEVICE_H
#define OPENTEC_BASE_USB_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/input_report.h"

enum {
    USB_DEVICE_REPORT_SIZE = 64,
    USB_DEVICE_HID_REPORT_INPUT = 1,
    USB_DEVICE_HID_REPORT_OUTPUT = 2,
    USB_DEVICE_HID_REPORT_FEATURE = 3,
};

typedef struct {
    uint8_t report_type;
    uint8_t report_id;
    uint8_t length;
    uint8_t data[USB_DEVICE_REPORT_SIZE];
} UsbDeviceOutputReport;

void usb_device_init(BoardVariant variant);
bool usb_device_set_input_mode(UsbInputReportMode mode);
UsbInputReportMode usb_device_input_mode(void);
void usb_device_service(void);
bool usb_device_configured(void);
bool usb_device_take_output(UsbDeviceOutputReport *report);
bool usb_device_send_input(const uint8_t *report, uint8_t length);

#endif
