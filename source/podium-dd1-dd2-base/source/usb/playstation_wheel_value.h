#ifndef OPENTEC_BASE_USB_PLAYSTATION_WHEEL_VALUE_H
#define OPENTEC_BASE_USB_PLAYSTATION_WHEEL_VALUE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"

typedef struct {
    uint8_t legacy_axes[2];
    uint32_t deadline_ms;
    bool release_pending;
} UsbPlaystationWheelValue;

void usb_playstation_wheel_value_init(UsbPlaystationWheelValue *value);
bool usb_playstation_wheel_value_apply(UsbPlaystationWheelValue *value,
                                       const UsbDeviceOutputReport *report, uint32_t now_ms);
bool usb_playstation_wheel_value_expire(UsbPlaystationWheelValue *value, uint32_t now_ms);
const uint8_t *usb_playstation_wheel_value_axes(const UsbPlaystationWheelValue *value);

#endif
