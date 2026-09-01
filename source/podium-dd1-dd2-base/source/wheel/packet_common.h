#ifndef OPENTEC_BASE_WHEEL_PACKET_COMMON_H
#define OPENTEC_BASE_WHEEL_PACKET_COMMON_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

/** @brief Common attached-wheel packet dimensions. */
enum {
    WHEEL_PACKET_COMMON_REQUEST_SIZE = 32,       /**< Request size in bytes. */
    WHEEL_PACKET_COMMON_RESPONSE_SIZE = 9,       /**< Response size in bytes. */
    WHEEL_PACKET_COMMON_SNAPSHOT_SIZE = 30,      /**< Normalized snapshot size in bytes. */
    WHEEL_PACKET_COMMON_BUTTON_COUNT = 3,        /**< Number of button bytes. */
    WHEEL_PACKET_COMMON_CONTROL_COUNT = 8,       /**< Number of control bytes. */
    WHEEL_PACKET_COMMON_AXIS_VALUE_COUNT = 2,    /**< Number of 16-bit axis values. */
    WHEEL_PACKET_COMMON_HISTORY_DEPTH = 3,       /**< Number of retained filter samples. */
    WHEEL_PACKET_COMMON_FILTERED_AXIS_COUNT = 2, /**< Number of filtered auxiliary axes. */
};

/** @brief Logical fields shared by the common 30-byte attached-wheel packet layouts. */
typedef struct {
    uint8_t buttons[WHEEL_PACKET_COMMON_BUTTON_COUNT]; /**< Filtered button bytes. */
    uint8_t axis_outputs[2];                           /**< Two primary axis-output bytes. */
    int8_t motion; /**< Primary rotary motion flags or normalized direction. */
    uint8_t controls[WHEEL_PACKET_COMMON_CONTROL_COUNT]; /**< Filtered control bytes. */
    uint8_t reserved_axes[2]; /**< Reserved axis bytes from the request. */
    uint16_t axis_values[WHEEL_PACKET_COMMON_AXIS_VALUE_COUNT]; /**< Two 16-bit axis values. */
    uint8_t mode_buttons;                                       /**< Secondary mode-button byte. */
    uint8_t axis_report_enabled;                                /**< Axis-report enable flag. */
    uint8_t auxiliary_data[4];                                  /**< Auxiliary packet data. */
    uint8_t report_mode;                                        /**< Report-mode selector. */
    uint8_t reserved_report;                                    /**< Reserved report byte. */
    uint8_t report_capabilities;                                /**< Report capability flags. */
    uint8_t axis_limit;                                         /**< Axis-limit value. */
} WheelPacketCommonInput;

/** @brief Three-sample button and analog-axis histories for common wheel packets. */
typedef struct {
    uint8_t button_samples[WHEEL_PACKET_COMMON_HISTORY_DEPTH]
                          [WHEEL_PACKET_COMMON_BUTTON_COUNT]; /**< Recent button samples. */
    uint8_t axis_samples[WHEEL_PACKET_COMMON_HISTORY_DEPTH]
                        [WHEEL_PACKET_COMMON_FILTERED_AXIS_COUNT]; /**< Recent axis samples. */
    uint8_t next_sample; /**< Index receiving the next sample. */
} WheelPacketCommonFilter;

/**
 * @brief Decodes a common attached-wheel request.
 *
 * Copies the request payload into the logical buttons, axes, motion, controls, and report fields.
 *
 * @param[in] request Thirty-two-byte request containing the command and payload.
 * @param[out] input Logical input state to populate.
 */
void wheel_packet_common_decode(const uint8_t request[WHEEL_PACKET_COMMON_REQUEST_SIZE],
                                WheelPacketCommonInput *input);

/**
 * @brief Initializes common packet filter histories.
 *
 * Clears button and auxiliary-axis samples and resets the insertion index.
 *
 * @param[out] filter Common filter state to initialize.
 */
void wheel_packet_common_filter_init(WheelPacketCommonFilter *filter);

/**
 * @brief Filters one common packet input sample.
 *
 * Retains button bits present across all recent samples, replaces the two auxiliary controls with
 * their three-sample means, and advances the shared history index.
 *
 * @param[in,out] filter Common filter state to update.
 * @param[in,out] input Input sample to filter in place.
 */
void wheel_packet_common_filter(WheelPacketCommonFilter *filter, WheelPacketCommonInput *input);

/**
 * @brief Expands packed common-packet controls.
 *
 * Maps the low and high nibbles of packed control byte seven into the first two common control
 * bytes in place.
 *
 * @param[in,out] input Common input whose packed controls are expanded.
 */
void wheel_packet_common_expand_packed_controls(WheelPacketCommonInput *input);

/**
 * @brief Applies common-packet button latching.
 *
 * Updates the persistent latch bits when enabled and no profile transition is pending.
 *
 * @param[in,out] input Common input and latch flags to update.
 * @param[in] enabled True when button latching is enabled.
 * @param[in] profile_transition_pending True while latch updates are suppressed.
 */
void wheel_packet_common_latch_buttons(WheelPacketCommonInput *input, bool enabled,
                                       bool profile_transition_pending);

/**
 * @brief Builds a common attached-wheel change snapshot.
 *
 * Serializes the logical input fields into the thirty-byte normalized request view.
 *
 * @param[in] input Common input to serialize.
 * @param[out] snapshot Thirty-byte snapshot destination.
 */
void wheel_packet_common_snapshot(const WheelPacketCommonInput *input,
                                  uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE]);

/**
 * @brief Encodes a common authenticated attached-wheel response.
 *
 * Serializes display, vibration, and legacy-axis values into the nine-byte response layout.
 *
 * @param[in] display Current display output.
 * @param[in] vibration Two vibration-channel values.
 * @param[in] legacy_axes Two legacy output-axis values.
 * @param[out] response Nine-byte response destination.
 */
void wheel_packet_common_response_encode(const WheelDisplayOutput *display,
                                         const uint8_t vibration[2], const uint8_t legacy_axes[2],
                                         uint8_t response[WHEEL_PACKET_COMMON_RESPONSE_SIZE]);

#endif
