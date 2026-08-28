#ifndef OPENTEC_BASE_USB_OUTPUT_COMMAND_H
#define OPENTEC_BASE_USB_OUTPUT_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"

typedef enum {
    USB_OUTPUT_COMMAND_SHORT,
    USB_OUTPUT_COMMAND_VENDOR_TRANSFER,
} UsbOutputCommandKind;

typedef struct {
    UsbOutputCommandKind kind;
    const uint8_t *payload;
    uint8_t length;
} UsbOutputCommand;

bool usb_output_command_decode(const UsbDeviceOutputReport *report, UsbOutputCommand *command);

#endif
