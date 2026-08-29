#ifndef OPENTEC_BASE_USB_XBOX_GIP_INPUT_H
#define OPENTEC_BASE_USB_XBOX_GIP_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/xbox_gip_response.h"

enum {
    USB_XBOX_GIP_WHEEL_BUTTON_COUNT = 3,
    USB_XBOX_GIP_WHEEL_CONTROL_COUNT = 8,
    USB_XBOX_GIP_ROTARY_COUNT = 5,
};

typedef struct {
    uint8_t buttons[USB_XBOX_GIP_WHEEL_BUTTON_COUNT];
    uint8_t mode_buttons;
    uint8_t controls[USB_XBOX_GIP_WHEEL_CONTROL_COUNT];
    uint8_t rotary[USB_XBOX_GIP_ROTARY_COUNT];
    uint16_t steering;
    uint16_t pedals[USB_XBOX_GIP_INPUT_PEDAL_COUNT];
    uint8_t auxiliary_pedal;
    uint8_t clutch_paddles[2];
    int8_t encoder_direction;
    uint8_t wheel_mode;
    uint8_t axis_mode;
    uint8_t led_state;
    uint16_t steering_range_degrees;
    uint8_t force_feedback_percent;
    bool pedal_active[USB_XBOX_GIP_INPUT_PEDAL_COUNT];
    bool auxiliary_pedal_active;
} UsbXboxGipInputState;

typedef struct {
    bool alternate_packet_bit;
} UsbXboxGipInputBuilder;

void usb_xbox_gip_input_builder_init(UsbXboxGipInputBuilder *builder);
void usb_xbox_gip_input_build(UsbXboxGipInputBuilder *builder, const UsbXboxGipInputState *state,
                              UsbXboxGipInputSnapshot *snapshot);

#endif
