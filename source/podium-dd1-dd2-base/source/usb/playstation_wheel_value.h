#ifndef OPENTEC_BASE_USB_PLAYSTATION_WHEEL_VALUE_H
#define OPENTEC_BASE_USB_PLAYSTATION_WHEEL_VALUE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"

typedef struct {
    uint8_t legacy_axes[2];
    uint32_t deadline_ms;
    bool release_pending;
    bool axis_copy_enabled;
} UsbPlaystationWheelValue;

void usb_playstation_wheel_value_init(UsbPlaystationWheelValue *value);
void usb_playstation_wheel_value_set(UsbPlaystationWheelValue *value, uint8_t low, uint8_t high,
                                     uint32_t now_ms);
bool usb_playstation_wheel_value_apply(UsbPlaystationWheelValue *value,
                                       const UsbDeviceOutputReport *report, uint32_t now_ms);
bool usb_playstation_wheel_value_expire(UsbPlaystationWheelValue *value, uint32_t now_ms);
void usb_playstation_wheel_value_set_axis_copy(UsbPlaystationWheelValue *value, bool enabled);
bool usb_playstation_wheel_value_copy_axes(UsbPlaystationWheelValue *value, const uint8_t axes[2]);
const uint8_t *usb_playstation_wheel_value_axes(const UsbPlaystationWheelValue *value);

#endif
