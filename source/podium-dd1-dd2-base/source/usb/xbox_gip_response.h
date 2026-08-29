#ifndef OPENTEC_BASE_USB_XBOX_GIP_RESPONSE_H
#define OPENTEC_BASE_USB_XBOX_GIP_RESPONSE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/console_descriptor.h"

enum {
    USB_XBOX_GIP_DIGEST_RESPONSE_SIZE = 32,
    USB_XBOX_GIP_READY_RESPONSE_SIZE = 8,
    USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE = 13,
    USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE = 17,
    USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE = 55,
    USB_XBOX_GIP_INPUT_RESPONSE_SIZE = 54,
    USB_XBOX_GIP_INPUT_PEDAL_COUNT = 3,
    USB_XBOX_GIP_INPUT_SELECTOR_COUNT = 6,
    USB_XBOX_GIP_INPUT_AUXILIARY_COUNT = 3,
};

typedef struct {
    uint8_t buttons[2];
    uint16_t steering;
    uint16_t pedals[USB_XBOX_GIP_INPUT_PEDAL_COUNT];
    uint8_t auxiliary_pedal;
    uint8_t axis_mode;
    uint8_t led_state;
    uint16_t steering_range_degrees;
    uint8_t force_feedback_level;
    bool pedal_active[USB_XBOX_GIP_INPUT_PEDAL_COUNT];
    bool auxiliary_pedal_active;
    uint8_t clutch_paddles[2];
    uint8_t selectors[USB_XBOX_GIP_INPUT_SELECTOR_COUNT];
    uint8_t button_flags;
    uint8_t packed_buttons;
    uint8_t auxiliary_buttons[USB_XBOX_GIP_INPUT_AUXILIARY_COUNT];
    uint8_t extended_button;
} UsbXboxGipInputSnapshot;

typedef struct {
    BoardVariant board_variant;
    uint8_t wheel_mode;
    uint8_t pedal_connection_flags;
    uint8_t auxiliary_axis_active;
    uint8_t axis_mode;
    uint8_t transfer_code;
    uint8_t multi_position_mode;
    bool hardware_option;
    bool h_pattern_available;
    bool legacy_pedal_mode;
    bool primary_pedal_calibration;
    bool secondary_pedal_calibration;
    bool pedal_recovery_handshake;
    bool thermal_effect_limit;
    bool wheel_calibration_available;
    bool wheel_input_capability_available;
    bool multi_position_supported;
    bool adapter_connected;
} UsbXboxGipExtendedStatus;

uint8_t usb_xbox_gip_sequence_take(uint8_t *next_sequence);
void usb_xbox_gip_digest_response_encode(BoardVariant variant, uint8_t wheel_mode, uint8_t sequence,
                                         const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                         uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE]);
void usb_xbox_gip_ready_response_encode(uint8_t sequence,
                                        uint8_t output[USB_XBOX_GIP_READY_RESPONSE_SIZE]);
void usb_xbox_gip_transfer_status_response_encode(
    uint8_t sequence, const uint8_t request[2],
    uint8_t output[USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE]);
void usb_xbox_gip_capability_response_encode(uint8_t sequence,
                                             uint8_t output[USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE]);
void usb_xbox_gip_extended_status_response_encode(
    uint8_t sequence, const UsbXboxGipExtendedStatus *status,
    uint8_t output[USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE]);
void usb_xbox_gip_input_response_encode(uint8_t sequence, const UsbXboxGipInputSnapshot *snapshot,
                                        uint8_t output[USB_XBOX_GIP_INPUT_RESPONSE_SIZE]);

#endif
