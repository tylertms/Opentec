#ifndef OPENTEC_BASE_USB_OPERATING_MODE_COMMAND_H
#define OPENTEC_BASE_USB_OPERATING_MODE_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"

enum { USB_OPERATING_MODE_PARAMETER_COUNT = 4 };

typedef struct {
    uint8_t opcode;
    uint8_t parameters[USB_OPERATING_MODE_PARAMETER_COUNT];
} UsbOperatingModeCommand;

bool usb_operating_mode_command_decode(const UsbOutputCommand *output,
                                       UsbOperatingModeCommand *command);
bool usb_operating_mode_command_requests_native_reset(const UsbOperatingModeCommand *command);

#endif
