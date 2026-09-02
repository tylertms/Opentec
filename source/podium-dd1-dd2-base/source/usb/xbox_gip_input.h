#ifndef OPENTEC_BASE_USB_XBOX_GIP_INPUT_H
#define OPENTEC_BASE_USB_XBOX_GIP_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/xbox_gip_response.h"
#include "wheel/adapter.h"

/** @brief Xbox GIP input source-count constants. */
enum {
    USB_XBOX_GIP_WHEEL_BUTTON_COUNT = 3,  /**< Number of source wheel button bytes. */
    USB_XBOX_GIP_WHEEL_CONTROL_COUNT = 8, /**< Number of source wheel control bytes. */
    USB_XBOX_GIP_ROTARY_COUNT = 5,        /**< Number of rotary selector values. */
};

/** @brief Normalized wheel input values used to build an Xbox GIP snapshot. */
typedef struct {
    uint8_t buttons[USB_XBOX_GIP_WHEEL_BUTTON_COUNT];   /**< Primary wheel button bytes. */
    uint8_t mode_buttons;                               /**< Mode-specific button byte. */
    uint8_t controls[USB_XBOX_GIP_WHEEL_CONTROL_COUNT]; /**< Secondary wheel controls. */
    uint8_t rotary[USB_XBOX_GIP_ROTARY_COUNT];          /**< Rotary selector values. */
    uint16_t steering;                                  /**< Current steering axis value. */
    uint16_t pedals[USB_XBOX_GIP_INPUT_PEDAL_COUNT];    /**< Current pedal axis values. */
    uint8_t auxiliary_pedal;                            /**< Current auxiliary-pedal value. */
    uint8_t clutch_paddles[2];                          /**< Current clutch-paddle values. */
    int8_t encoder_direction;      /**< Signed rotary encoder direction event. */
    uint8_t wheel_mode;            /**< Attached-wheel protocol mode. */
    uint8_t axis_mode;             /**< Current axis mode. */
    uint8_t led_state;             /**< Current wheel LED state. */
    uint16_t steering_range_units; /**< Current steering range in protocol units. */
    uint8_t force_feedback_level;  /**< Current force-feedback level in the protocol byte scale. */
    bool pedal_active[USB_XBOX_GIP_INPUT_PEDAL_COUNT]; /**< Whether each pedal axis is active. */
    bool auxiliary_pedal_active; /**< Whether the auxiliary pedal axis is active. */
    bool shifter_transitions[2]; /**< Rising transition state for two shifter controls. */
} UsbXboxGipInputState;

/** @brief Stateful input snapshot builder. */
typedef struct {
    bool alternate_packet_bit; /**< Alternating packet bit used in primary button output. */
    uint8_t led_input;         /**< Most recently sampled LED input. */
    uint8_t led_encoded;       /**< Retained LED value encoded into snapshots. */
} UsbXboxGipInputBuilder;

/**
 * @brief Initializes Xbox GIP input composition.
 *
 * Clears the alternating packet state so the first built snapshot asserts its packet bit.
 *
 * @param[out] builder Input builder to initialize.
 */
void usb_xbox_gip_input_builder_init(UsbXboxGipInputBuilder *builder);

/**
 * @brief Builds a logical Xbox GIP input snapshot.
 *
 * Maps normalized wheel buttons and extension controls, copies the live steering and pedal axes,
 * lays out rotary selector groups around the signed encoder event, and copies the runtime
 * force-feedback level in the protocol byte range.
 *
 * @param[in,out] builder Input builder containing the alternating packet state.
 * @param[in] state Current normalized wheel, pedal, profile, and shifter state.
 * @param[out] snapshot Logical Xbox GIP snapshot.
 */
void usb_xbox_gip_input_build(UsbXboxGipInputBuilder *builder, const UsbXboxGipInputState *state,
                              UsbXboxGipInputSnapshot *snapshot);

/**
 * @brief Merges attached-adapter buttons into an Xbox GIP snapshot.
 *
 * Applies the four vendor wheel-mode mappings from the official status builder after base button
 * composition. Adapter bits therefore have the official priority, and tuning suppression clears
 * the same face-button fields as the firmware.
 *
 * @param[in,out] snapshot Xbox GIP snapshot containing the base button mapping.
 * @param[in] wheel_mode Attached-wheel protocol mode.
 * @param[in] adapter Attached-adapter input state.
 * @param[in] suppress_base_buttons True while tuning interaction suppresses face-button output.
 */
void usb_xbox_gip_input_merge_adapter_buttons(UsbXboxGipInputSnapshot *snapshot, uint8_t wheel_mode,
                                              const WheelAdapterInput *adapter,
                                              bool suppress_base_buttons);

#endif
