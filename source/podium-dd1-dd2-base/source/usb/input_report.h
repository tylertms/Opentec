#ifndef OPENTEC_BASE_USB_INPUT_REPORT_H
#define OPENTEC_BASE_USB_INPUT_REPORT_H

#include <stdint.h>

#include "usb/fanatec_input.h"
#include "usb/logitech_input.h"

enum {
    USB_INPUT_REPORT_MAX_SIZE = FANATEC_INPUT_REPORT_SIZE,
};

typedef enum {
    USB_INPUT_REPORT_MODE_FANATEC = 0,
    USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY = 1,
    USB_INPUT_REPORT_MODE_DRIVING_FORCE_EX = 2,
    USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO = 3,
    USB_INPUT_REPORT_MODE_G27 = 4,
} UsbInputReportMode;

typedef struct {
    fanatec_input_state fanatec;
    LogitechInputState logitech;
} UsbInputReportState;

uint8_t usb_input_report_encode(UsbInputReportMode mode, uint8_t report[USB_INPUT_REPORT_MAX_SIZE],
                                const UsbInputReportState *state);

#endif
