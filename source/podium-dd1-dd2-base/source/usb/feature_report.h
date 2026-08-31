#ifndef OPENTEC_BASE_USB_FEATURE_REPORT_H
#define OPENTEC_BASE_USB_FEATURE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/device.h"

typedef struct {
    uint16_t status;
    uint8_t wheel_mode;
    uint8_t pedal_active;
    uint8_t auxiliary_profile;
    uint8_t axis_modes[2];
    uint8_t transfer_code;
    uint8_t rotary_mode;
    bool pedal_legacy;
    bool pedal_io_active;
    bool pedal_handshake_active;
    bool resistance_active;
    bool pedal_calibration_active;
    bool wheel_calibration_available;
    bool wheel_axis_report_enabled;
    bool adapter_connected;
} UsbFeatureReport31State;

typedef struct {
    uint8_t positions[3];
    int8_t events[3];
    int8_t pulse_directions[4];
    uint8_t extended_buttons;
    uint8_t auxiliary_buttons[3];
    uint8_t rotary_mode;
    bool tertiary_active;
    bool remap_selectors;
} UsbFeatureReport33State;

void usb_feature_report_31_encode(const UsbFeatureReport31State *state,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]);
void usb_feature_report_32_encode(const TuningProfileBank *bank, bool dirty,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]);
void usb_feature_report_33_encode(const UsbFeatureReport33State *state,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]);
void usb_feature_report_36_encode(uint8_t page, uint8_t output[USB_DEVICE_REPORT_SIZE]);

#endif
