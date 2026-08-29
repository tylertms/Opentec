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

/**
 * @brief Runtime services selected by operating-mode commands.
 *
 * Identifies the normal application, auxiliary paths, bridge paths, recovery paths, and reset
 * transition without exposing their internal service layouts.
 */
typedef enum {
    USB_RUNTIME_MODE_NORMAL = 0,
    USB_RUNTIME_MODE_AUXILIARY = 1,
    USB_RUNTIME_MODE_AUXILIARY_RECOVERY = 2,
    USB_RUNTIME_MODE_STATUS_BRIDGE = 3,
    USB_RUNTIME_MODE_USB_BRIDGE = 4,
    USB_RUNTIME_MODE_PROTOCOL_BRIDGE = 5,
    USB_RUNTIME_MODE_PROTOCOL_RECOVERY = 6,
    USB_RUNTIME_MODE_RESET = 7,
} UsbRuntimeMode;

/**
 * @brief Accepted runtime transition and its required persistence action.
 *
 * Carries the selected runtime service and whether settings must be stored before entering it.
 */
typedef struct {
    UsbRuntimeMode mode;
    bool save_settings;
} UsbRuntimeModeTransition;

bool usb_operating_mode_command_decode(const UsbOutputCommand *output,
                                       UsbOperatingModeCommand *command);
bool usb_operating_mode_command_requests_native_reset(const UsbOperatingModeCommand *command);
bool usb_operating_mode_command_decode_status(const UsbOperatingModeCommand *command,
                                              bool *enabled);
bool usb_operating_mode_command_requests_led_pattern(const UsbOperatingModeCommand *command);
bool usb_operating_mode_command_decode_runtime(const UsbOperatingModeCommand *command,
                                               uint8_t interface_mode, uint8_t wheel_mode,
                                               uint8_t auxiliary_state,
                                               UsbRuntimeModeTransition *transition);

#endif
