#ifndef OPENTEC_BASE_WHEEL_PACKET_MODE_ONE_H
#define OPENTEC_BASE_WHEEL_PACKET_MODE_ONE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

/** @brief Mode-one packet dimensions and field counts. */
enum {
    WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE = 9,              /**< Response size in bytes. */
    WHEEL_PACKET_MODE_ONE_REQUEST_SIZE = 32,              /**< Request size in bytes. */
    WHEEL_PACKET_MODE_ONE_SNAPSHOT_SIZE = 30,             /**< Snapshot size in bytes. */
    WHEEL_PACKET_MODE_ONE_BUTTON_COUNT = 3,               /**< Number of button bytes. */
    WHEEL_PACKET_MODE_ONE_BUTTON_HISTORY_DEPTH = 3,       /**< Button-history sample count. */
    WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_COUNT = 2,         /**< Number of control axes. */
    WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH = 3, /**< Control-axis sample count. */
    WHEEL_PACKET_MODE_ONE_AXIS_OUTPUT_COUNT = 2,          /**< Number of axis-output bytes. */
    WHEEL_PACKET_MODE_ONE_AXIS_VALUE_COUNT = 2,           /**< Number of 16-bit axis values. */
};

/** @brief Control values and latches decoded from a mode-one request. */
typedef struct {
    uint8_t values[2];     /**< Two control values. */
    uint8_t enabled;       /**< Control enable flag. */
    uint8_t latch_flags;   /**< Button-latch flags. */
    uint8_t x;             /**< First analog control axis. */
    uint8_t y;             /**< Second analog control axis. */
    uint8_t mode;          /**< Control mode selector. */
    uint8_t packed_values; /**< Packed control values. */
} WheelPacketModeOneControls;

/** @brief Logical input fields decoded from a mode-one request. */
typedef struct {
    uint8_t buttons[WHEEL_PACKET_MODE_ONE_BUTTON_COUNT];           /**< Button bytes. */
    uint8_t axis_outputs[WHEEL_PACKET_MODE_ONE_AXIS_OUTPUT_COUNT]; /**< Axis-output bytes. */
    int8_t motion;                                                 /**< Primary motion flags. */
    WheelPacketModeOneControls controls; /**< Decoded controls and latch flags. */
    uint16_t axis_values[WHEEL_PACKET_MODE_ONE_AXIS_VALUE_COUNT]; /**< 16-bit axis values. */
    uint8_t mode_buttons;        /**< Secondary mode-button byte. */
    uint8_t axis_report_enabled; /**< Axis-report enable flag. */
    uint8_t report_mode;         /**< Report-mode selector. */
    uint8_t report_capabilities; /**< Report capability flags. */
    uint8_t axis_limit;          /**< Axis-limit value. */
} WheelPacketModeOneInput;

/** @brief Report fields retained separately during mode-one normalization. */
typedef struct {
    uint16_t axis_values[WHEEL_PACKET_MODE_ONE_AXIS_VALUE_COUNT]; /**< 16-bit axis values. */
    uint8_t report_mode;                                          /**< Report-mode selector. */
    uint8_t report_capabilities;                                  /**< Report capability flags. */
    uint8_t axis_limit;                                           /**< Axis-limit value. */
} WheelPacketModeOneReportState;

/** @brief Three-sample button history for mode-one filtering. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_MODE_ONE_BUTTON_HISTORY_DEPTH]
                   [WHEEL_PACKET_MODE_ONE_BUTTON_COUNT]; /**< Recent button samples. */
    uint8_t next_sample;                                 /**< Index receiving the next sample. */
} WheelPacketModeOneButtonFilter;

/** @brief Three-sample control-axis history for mode-one filtering. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH]
                   [WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_COUNT]; /**< Recent control-axis samples. */
    uint8_t next_sample; /**< Index receiving the next sample. */
} WheelPacketModeOneControlAxisFilter;

/** @brief Display, vibration, and axis fields for a mode-one response. */
typedef struct {
    WheelDisplayOutput display; /**< Display output. */
    uint8_t vibration[2];       /**< Two vibration-channel values. */
    uint8_t legacy_axes[2];     /**< Two legacy output-axis values. */
} WheelPacketModeOneOutput;

/**
 * @brief Reports whether a wheel mode uses mode-one packets.
 *
 * Selects the standard, vendor, and authenticated variants sharing this packet layout.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for modes 1, 2, 3, 0x13, 0x14, or 0x16; otherwise false.
 */
bool wheel_packet_mode_one_applies(uint8_t wheel_mode);

/**
 * @brief Initializes mode-one button filtering.
 *
 * Clears the three-sample button history and resets its insertion index.
 *
 * @param[out] filter Button filter state to initialize.
 */
void wheel_packet_mode_one_button_filter_init(WheelPacketModeOneButtonFilter *filter);

/**
 * @brief Filters one mode-one button sample.
 *
 * Retains button bits present across all three recent samples and updates the input in place.
 *
 * @param[in,out] filter Button history to update.
 * @param[in,out] input Input sample to filter in place.
 */
void wheel_packet_mode_one_filter_buttons(WheelPacketModeOneButtonFilter *filter,
                                          WheelPacketModeOneInput *input);

/**
 * @brief Initializes mode-one control-axis filtering.
 *
 * Clears the three-sample control-axis history and resets its insertion index.
 *
 * @param[out] filter Control-axis filter state to initialize.
 */
void wheel_packet_mode_one_control_axis_filter_init(WheelPacketModeOneControlAxisFilter *filter);

/**
 * @brief Filters one mode-one control-axis sample.
 *
 * Replaces both control axes with their unsigned three-sample moving averages.
 *
 * @param[in,out] filter Control-axis history to update.
 * @param[in,out] input Input sample whose axes are filtered in place.
 */
void wheel_packet_mode_one_filter_control_axes(WheelPacketModeOneControlAxisFilter *filter,
                                               WheelPacketModeOneInput *input);

/**
 * @brief Decodes a mode-one attached-wheel request.
 *
 * Copies the request payload into the logical button, axis, control, and report fields.
 *
 * @param[in] request Thirty-two-byte mode-one request.
 * @param[out] input Mode-one input state to populate.
 */
void wheel_packet_mode_one_decode(const uint8_t request[WHEEL_PACKET_MODE_ONE_REQUEST_SIZE],
                                  WheelPacketModeOneInput *input);

/**
 * @brief Normalizes mode-one input and builds its snapshot.
 *
 * Applies authenticated button latching, clears unsupported fields, and writes the thirty-byte
 * change-detection snapshot.
 *
 * @param[in,out] input Decoded mode-one input to normalize.
 * @param[in] authenticated True for authenticated mode-one variants.
 * @param[in] button_latch_enabled True to enable authenticated button latching.
 * @param[in] profile_transition_pending True while latching is suppressed.
 * @param[out] snapshot Thirty-byte normalized snapshot destination.
 */
void wheel_packet_mode_one_normalize(WheelPacketModeOneInput *input, bool authenticated,
                                     bool button_latch_enabled, bool profile_transition_pending,
                                     uint8_t snapshot[WHEEL_PACKET_MODE_ONE_SNAPSHOT_SIZE]);

/**
 * @brief Encodes a mode-one attached-wheel response.
 *
 * Writes the mode-appropriate command, display output, vibration channels, and legacy axes.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] output Mode-one response output state.
 * @param[out] response Nine-byte response destination.
 */
void wheel_packet_mode_one_encode(uint8_t wheel_mode, const WheelPacketModeOneOutput *output,
                                  uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE]);

#endif
