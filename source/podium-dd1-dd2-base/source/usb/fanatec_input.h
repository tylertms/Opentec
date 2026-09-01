#ifndef OPENTEC_BASE_USB_FANATEC_INPUT_H
#define OPENTEC_BASE_USB_FANATEC_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "shifter/h_pattern.h"
#include "shifter/input.h"
#include "wheel/axis_override.h"

/** @brief Fanatec input report identifiers, sizes, and field counts. */
enum {
    FANATEC_INPUT_REPORT_ID = 1,                  /**< Native Fanatec input report identifier. */
    FANATEC_INPUT_REPORT_SIZE = 34,               /**< Native Fanatec input report size in bytes. */
    FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE = 33, /**< Compatibility payload size in bytes. */
    FANATEC_INPUT_BUTTON_BANKS = 5,               /**< Number of encoded button banks. */
    FANATEC_INPUT_ROTARY_BYTES = 5,               /**< Number of encoded rotary bytes. */
    FANATEC_INPUT_MULTI_POSITION_CHANNELS = 3,    /**< Number of multi-position channels. */
    FANATEC_INPUT_ACCESSORY_BYTES = 5,            /**< Number of encoded accessory bytes. */
    FANATEC_INPUT_PEDAL_AXES = 3,                 /**< Number of pedal axes. */
    FANATEC_INPUT_DIRECT_DRIVE_MODE = 0xfe        /**< Wheel-mode code for direct-drive hardware. */
};

/** @brief Logical button, rotary, accessory, axis, and status fields in a Fanatec input report. */
typedef struct {
    uint8_t button_banks[FANATEC_INPUT_BUTTON_BANKS]; /**< Encoded button banks. */
    uint8_t rotary[FANATEC_INPUT_ROTARY_BYTES];       /**< Encoded rotary fields. */
    uint8_t accessory[FANATEC_INPUT_ACCESSORY_BYTES]; /**< Encoded accessory fields. */
    uint16_t steering;                                /**< Steering axis value. */
    uint16_t pedals[FANATEC_INPUT_PEDAL_AXES];        /**< Pedal axis values. */
    uint8_t clutch_paddles[2];                        /**< Clutch-paddle axis values. */
    uint8_t auxiliary_pedal;                          /**< Auxiliary pedal axis value. */
    int8_t encoder_position;                          /**< Signed rotary encoder position. */
    uint8_t transfer_code;                            /**< Attached-wheel transfer code. */
    uint8_t status_flags;       /**< Wheel, shifter, pedal, and thermal status bits. */
    uint8_t wheel_mode;         /**< Attached-wheel operating mode. */
    uint8_t axis_limit;         /**< Steering axis limit. */
    uint8_t bite_point_percent; /**< Pending bite-point percentage. */
    bool bite_point_update; /**< True when the next native report carries a bite-point update. */
} fanatec_input_state;

/** @brief One logical multi-position rotary channel and its pending event. */
typedef struct {
    uint8_t position; /**< One-based rotary position. */
    uint8_t event;    /**< Pending rotary event value. */
    bool active;      /**< True when the channel contributes to the report. */
} fanatec_multi_position_channel;

/** @brief Multi-position rotary channels and selector remapping state. */
typedef struct {
    fanatec_multi_position_channel
        channels[FANATEC_INPUT_MULTI_POSITION_CHANNELS]; /**< Rotary channels. */
    bool remap_selectors; /**< True when the alternate selector layout is active. */
} fanatec_multi_position_input;

/**
 * @brief Encodes the native Fanatec input report.
 *
 * Writes report identifier one followed by the complete Fanatec input payload.
 *
 * @param[out] report Buffer that receives the encoded report.
 * @param[in] state Logical Fanatec input values.
 * @return True when both pointers are valid and the report was encoded; otherwise false.
 */
bool fanatec_input_encode(uint8_t report[FANATEC_INPUT_REPORT_SIZE],
                          const fanatec_input_state *state);

/**
 * @brief Encodes the Fanatec compatibility input report.
 *
 * Writes the complete Fanatec input payload without a leading HID report identifier.
 *
 * @param[out] report Buffer that receives the encoded report.
 * @param[in] state Logical Fanatec input values.
 * @return True when both pointers are valid and the report was encoded; otherwise false.
 */
bool fanatec_input_compatibility_encode(uint8_t report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE],
                                        const fanatec_input_state *state);

/**
 * @brief Applies attached-wheel controls to the Fanatec input fields.
 *
 * Copies the standard rotary controls and, when enabled, the extended rotary and accessory
 * controls.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] controls Eight attached-wheel control bytes.
 * @param[in] include_extended True to include extended control bytes.
 */
void fanatec_input_apply_wheel_controls(fanatec_input_state *state, const uint8_t controls[8],
                                        bool include_extended);

/**
 * @brief Applies attached-wheel accessory flags to the Fanatec input state.
 *
 * Replaces the low nibble of the final accessory byte while preserving its high-nibble mode bits.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] flags Attached-wheel accessory flags.
 */
void fanatec_input_apply_wheel_accessory(fanatec_input_state *state, uint8_t flags);

/**
 * @brief Applies alternative-shifter status to the Fanatec input state.
 *
 * Replaces bit seven of the final accessory byte while preserving all other accessory flags.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] enabled True while alternative-shifter mode is active.
 */
void fanatec_input_apply_alternative_shifter(fanatec_input_state *state, bool enabled);

/**
 * @brief Applies the multi-position reporting mode to the Fanatec input state.
 *
 * Replaces bits four and five of the final accessory byte with the low two bits of the effective
 * mode.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] mode Effective multi-position reporting mode.
 */
void fanatec_input_apply_multi_position_mode(fanatec_input_state *state, uint8_t mode);

/**
 * @brief Applies multi-position rotary values to the Fanatec input state.
 *
 * Encodes event, pulse, or constant selector values according to the selected reporting mode and
 * clears the rotary fields before encoding them.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] mode Effective multi-position reporting mode.
 * @param[in] input Logical rotary channels and selector layout.
 */
void fanatec_input_apply_multi_position_rotaries(fanatec_input_state *state, uint8_t mode,
                                                 const fanatec_multi_position_input *input);

/**
 * @brief Applies shifter input to the Fanatec button fields.
 *
 * Places the current H-pattern gear or sequential transition buttons according to both shifter
 * port modes.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] shifter Mode and transition state for both shifter ports.
 * @param[in] gear Current H-pattern gear bit, or neutral.
 */
void fanatec_input_apply_shifter(fanatec_input_state *state, const ShifterInputState *shifter,
                                 ShifterGear gear);

/**
 * @brief Applies the thermal effect-limit indication to the Fanatec input state.
 *
 * Replaces status bit four with the current effect-strength limit state.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] active True while thermal management limits effect strengths.
 */
void fanatec_input_apply_thermal_limit(fanatec_input_state *state, bool active);

/**
 * @brief Applies pedal transport status to the Fanatec input state.
 *
 * Replaces status bits one through five with the current pedal transport states.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] legacy True while legacy pedal transport is active.
 * @param[in] auxiliary True while the auxiliary pedal profile is active.
 * @param[in] handshake True while the modern pedal startup handshake is active.
 * @param[in] resistance True while pedal resistance adjustment is active.
 * @param[in] calibration True while pedal calibration is active.
 */
void fanatec_input_apply_pedal_status(fanatec_input_state *state, bool legacy, bool auxiliary,
                                      bool handshake, bool resistance, bool calibration);

/**
 * @brief Applies attached-wheel calibration availability to the Fanatec input state.
 *
 * Replaces status bit six with the effective wheel calibration capability.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] available True when the attached wheel exposes calibration controls.
 */
void fanatec_input_apply_wheel_calibration(fanatec_input_state *state, bool available);

/**
 * @brief Applies attached-wheel input capability to the Fanatec input state.
 *
 * Replaces status bit seven with the effective wheel input capability.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] available True when the attached wheel exposes its latched input capability.
 */
void fanatec_input_apply_wheel_input_capability(fanatec_input_state *state, bool available);

/**
 * @brief Merges attached-wheel axes into the Fanatec pedal fields.
 *
 * Expands enabled wheel axis bytes across the sixteen-bit pedal range and retains the lower value
 * from each wheel and pedal source.
 *
 * @param[in,out] state Input report state whose pedal fields receive merged values.
 * @param[in] overrides Attached-wheel axis channels and availability flags.
 */
void fanatec_input_apply_wheel_axis_overrides(fanatec_input_state *state,
                                              const WheelAxisOverrides *overrides);

/**
 * @brief Applies a live bite-point update to the native Fanatec report.
 *
 * Marks the next native report to carry the bite-point update marker and percentage.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] percent Current bite-point percentage.
 */
void fanatec_input_apply_bite_point_update(fanatec_input_state *state, uint8_t percent);

#endif
