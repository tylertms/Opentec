#ifndef OPENTEC_BASE_USB_UPDATER_IDENTITY_H
#define OPENTEC_BASE_USB_UPDATER_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"
#include "usb/updater_protocol.h"

enum { USB_UPDATER_IDENTITY_AUTOMATIC = 0xff };

/** @brief Runtime and attached-wheel state used to select an updater identity. */
typedef struct {
    UsbRuntimeMode runtime_mode;
    uint8_t wheel_mode;
    uint8_t response_selector;
    bool adapter_connected;
} UsbUpdaterIdentityInput;

void usb_updater_identity_select(const UsbUpdaterIdentityInput *input,
                                 uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE]);

#endif
