#ifndef OPENTEC_BASE_PEDAL_INPUT_H
#define OPENTEC_BASE_PEDAL_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"

/**
 * @brief Sizes and report identifiers used by pedal input decoding.
 */
enum {
    PEDAL_INPUT_AXIS_COUNT = 3,      /**< Number of published pedal axes. */
    PEDAL_FRAME_AXIS_SAMPLE = 1,     /**< Framed report containing pedal axis samples. */
    PEDAL_V3_BRAKE_FORCE_REPORT = 7, /**< V3 report containing alternate brake force. */
};

/**
 * @brief Stores the published pedal axes and auxiliary input.
 */
typedef struct {
    uint16_t axes[PEDAL_INPUT_AXIS_COUNT]; /**< Published pedal-axis values. */
    uint8_t auxiliary;                     /**< Published auxiliary input value. */
} PedalInput;

/**
 * @brief Stores V3 protocol reports that affect pedal input state.
 *
 * The state retains raw brake data, connection and calibration flags, and shared-axis values that
 * are consumed by the service layer.
 */
typedef struct {
    uint16_t raw_brake;            /**< Most recent raw V3 brake-axis sample. */
    uint8_t connection_flags;      /**< V3 pedal connection flags. */
    uint8_t alternate_brake_force; /**< Most recent alternate brake-force percentage. */
    uint8_t shared_axes[PEDAL_INPUT_AXIS_COUNT]; /**< Most recent shared-axis report values. */
    bool primary_calibration;   /**< True after a primary calibration mode report. */
    bool legacy_calibration;    /**< True after a legacy calibration mode report. */
    bool secondary_calibration; /**< True after a secondary calibration mode report. */
} PedalV3State;

/**
 * @brief Releases all published pedal inputs.
 *
 * Sets every axis and the auxiliary value to the internal released value.
 *
 * @param[out] input Pedal input state to release.
 */
void pedal_input_release(PedalInput *input);

/**
 * @brief Decodes a framed pedal axis-sample report.
 *
 * Copies the three little-endian axis values and auxiliary byte for an axis-sample report and
 * leaves input unchanged for other report types.
 *
 * @param[in] frame Decoded pedal frame.
 * @param[in,out] input Pedal input state to update; unchanged for non-axis reports.
 * @return True when frame is an axis-sample report.
 */
bool pedal_input_decode(const PedalFrame *frame, PedalInput *input);

/**
 * @brief Initializes V3 pedal report state.
 *
 * Clears raw samples, flags, calibration state, and shared-axis values.
 *
 * @param[out] state V3 state to initialize.
 */
void pedal_v3_state_init(PedalV3State *state);

/**
 * @brief Applies a recognized V3 report to pedal state.
 *
 * Updates report-specific V3 state and published axes while preserving a locked auxiliary input.
 *
 * @param[in] frame Decoded V3 pedal frame.
 * @param[in] auxiliary_locked True when another source owns the auxiliary input.
 * @param[in,out] state V3 state to update.
 * @param[in,out] input Published pedal input to update for axis reports.
 * @return True when frame type is recognized by the V3 input protocol.
 */
bool pedal_v3_apply_report(const PedalFrame *frame, bool auxiliary_locked, PedalV3State *state,
                           PedalInput *input);

/**
 * @brief Applies V3 brake-force scaling to a raw brake sample.
 *
 * Converts the encoded force control to a gain, scales value, and saturates the result to the
 * sixteen-bit published axis range.
 *
 * @param[in] value Raw brake-axis sample.
 * @param[in] force_percent Encoded signed brake-force control value.
 * @return Saturated scaled brake-axis sample.
 */
uint16_t pedal_input_scale_brake(uint16_t value, uint8_t force_percent);

/**
 * @brief Converts an internal pedal axis to its HID value.
 *
 * Complements all sixteen bits of value.
 *
 * @param[in] value Internal pedal-axis value.
 * @return Complemented HID axis value.
 */
uint16_t pedal_input_hid_axis(uint16_t value);

/**
 * @brief Converts an internal auxiliary value to its HID value.
 *
 * Complements all eight bits of value.
 *
 * @param[in] value Internal auxiliary value.
 * @return Complemented HID auxiliary value.
 */
uint8_t pedal_input_hid_auxiliary(uint8_t value);

#endif
