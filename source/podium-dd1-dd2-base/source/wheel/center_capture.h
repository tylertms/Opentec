#ifndef OPENTEC_BASE_WHEEL_CENTER_CAPTURE_H
#define OPENTEC_BASE_WHEEL_CENTER_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"

typedef enum {
    WHEEL_CENTER_CAPTURE_UNHANDLED,
    WHEEL_CENTER_CAPTURE_HANDLED,
    WHEEL_CENTER_CAPTURE_REQUESTED,
} WheelCenterCaptureAction;

typedef struct {
    uint32_t next_capture_ms;
    uint32_t next_notification_ms;
} WheelCenterCaptureCommand;

void wheel_center_capture_command_init(WheelCenterCaptureCommand *command);
WheelCenterCaptureAction wheel_center_capture_command_apply(WheelCenterCaptureCommand *command,
                                                            const UsbOutputCommand *output,
                                                            uint32_t now_ms);
bool wheel_center_capture_command_notification_due(WheelCenterCaptureCommand *command,
                                                   uint32_t now_ms);

#endif
