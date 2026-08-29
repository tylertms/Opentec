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

/** @brief Clutch inputs used to produce the two PlayStation controller axes. */
typedef struct {
    uint8_t wheel_mode;
    uint8_t paddle_mode;
    uint8_t wheel_axes[2];
    uint8_t adapter_axes[2];
    bool wheel_axis_enabled;
    bool adapter_connected;
} UsbPlaystationClutchInput;

/** @brief Attached-wheel controls used to produce PlayStation buttons. */
typedef struct {
    uint16_t secondary_buttons;
    uint16_t adapter_mode;
    uint8_t wheel_mode;
    uint8_t directional_buttons;
    uint8_t adapter_buttons[3];
    uint8_t auxiliary_buttons[2];
    uint8_t auxiliary_history;
    uint8_t extended_buttons;
    uint8_t axis_modes[2];
    bool adapter_connected;
    bool hat_suppressed;
    bool system_button_suppressed;
} UsbPlaystationButtonInput;

/** @brief Retained timing state for PlayStation button mapping. */
typedef struct {
    uint32_t system_button_deadline_ms;
    bool system_button_hold_active;
} UsbPlaystationInputMapper;

void usb_playstation_input_mapper_init(UsbPlaystationInputMapper *mapper);
uint8_t usb_playstation_input_map_hat(uint8_t directional_buttons);
void usb_playstation_input_map_clutch(uint8_t axes[2], const UsbPlaystationClutchInput *input);
bool usb_playstation_input_map_buttons(UsbPlaystationInputMapper *mapper,
                                       const UsbPlaystationButtonInput *input, uint32_t now_ms,
                                       UsbPlaystationInputState *state);
bool usb_playstation_input_encode(uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE],
                                  const UsbPlaystationInputState *state);

#endif
