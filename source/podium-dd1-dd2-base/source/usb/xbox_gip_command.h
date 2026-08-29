#ifndef OPENTEC_BASE_USB_XBOX_GIP_COMMAND_H
#define OPENTEC_BASE_USB_XBOX_GIP_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    USB_XBOX_GIP_COMMAND_SCRIPT_SAMPLES,
    USB_XBOX_GIP_COMMAND_SCRIPT_SLOT,
    USB_XBOX_GIP_COMMAND_SCRIPT_STATUS,
    USB_XBOX_GIP_COMMAND_SCRIPT_VALUES,
    USB_XBOX_GIP_COMMAND_SCRIPT_AXES,
} UsbXboxGipCommandKind;

typedef struct {
    UsbXboxGipCommandKind kind;
    uint16_t parameter;
} UsbXboxGipCommand;

bool usb_xbox_gip_command_decode(const uint8_t *packet, size_t length, UsbXboxGipCommand *command);

#endif
