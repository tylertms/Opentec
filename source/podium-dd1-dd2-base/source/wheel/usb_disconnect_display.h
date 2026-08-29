#ifndef OPENTEC_BASE_WHEEL_USB_DISCONNECT_DISPLAY_H
#define OPENTEC_BASE_WHEEL_USB_DISCONNECT_DISPLAY_H

#include <stdbool.h>

#include "wheel/display_output.h"

typedef struct {
    bool owns_output;
} UsbDisconnectDisplay;

void usb_disconnect_display_init(UsbDisconnectDisplay *display);
bool usb_disconnect_display_update(UsbDisconnectDisplay *display, bool visible,
                                   WheelDisplayOutput *output);

#endif
