#ifndef OPENTEC_BASE_USB_DEVICE_H
#define OPENTEC_BASE_USB_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"

enum { USB_DEVICE_REPORT_SIZE = 64 };

typedef struct {
    uint8_t report_type;
    uint8_t report_id;
    uint8_t length;
    uint8_t data[USB_DEVICE_REPORT_SIZE];
} UsbDeviceOutputReport;

void usb_device_init(BoardVariant variant);
void usb_device_service(void);
bool usb_device_configured(void);
bool usb_device_take_output(UsbDeviceOutputReport *report);
bool usb_device_send_input(const uint8_t *report, uint8_t length);

#endif
