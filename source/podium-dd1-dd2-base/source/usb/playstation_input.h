#ifndef OPENTEC_BASE_USB_PLAYSTATION_INPUT_H
#define OPENTEC_BASE_USB_PLAYSTATION_INPUT_H

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_PLAYSTATION_INPUT_REPORT_SIZE = 64,
    USB_PLAYSTATION_INPUT_PEDAL_COUNT = 3,
};

/** @brief Logical controls and axes carried by the PlayStation input report. */
typedef struct {
    uint8_t clutch_axes[2];
    uint8_t hat;
    uint16_t buttons;
    uint8_t vendor_buttons;
    uint16_t steering;
    uint16_t pedals[USB_PLAYSTATION_INPUT_PEDAL_COUNT];
    uint8_t wheel_hat;
    uint16_t auxiliary_axis;
} UsbPlaystationInputState;

bool usb_playstation_input_encode(uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE],
                                  const UsbPlaystationInputState *state);

#endif
