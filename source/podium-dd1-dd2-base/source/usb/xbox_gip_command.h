#ifndef OPENTEC_BASE_USB_XBOX_GIP_COMMAND_H
#define OPENTEC_BASE_USB_XBOX_GIP_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    USB_XBOX_GIP_COMMAND_CAPABILITIES,
    USB_XBOX_GIP_COMMAND_STEERING_RANGE,
    USB_XBOX_GIP_COMMAND_FORCE_FEEDBACK_STRENGTH,
    USB_XBOX_GIP_COMMAND_HOST_CAPABILITY,
    USB_XBOX_GIP_COMMAND_TRANSFER_STATUS,
    USB_XBOX_GIP_COMMAND_SCRIPT_SAMPLES,
    USB_XBOX_GIP_COMMAND_SCRIPT_SLOT,
    USB_XBOX_GIP_COMMAND_SCRIPT_STATUS,
    USB_XBOX_GIP_COMMAND_SCRIPT_VALUES,
    USB_XBOX_GIP_COMMAND_SCRIPT_AXES,
    USB_XBOX_GIP_COMMAND_EXTENDED_STATUS,
} UsbXboxGipCommandKind;

typedef struct {
    UsbXboxGipCommandKind kind;
    uint16_t parameter;
} UsbXboxGipCommand;

bool usb_xbox_gip_command_decode(const uint8_t *packet, size_t length, UsbXboxGipCommand *command);
uint16_t usb_xbox_gip_steering_range_normalize(uint16_t requested_degrees);
uint8_t usb_xbox_gip_force_feedback_strength_normalize(uint8_t requested_level);

#endif
