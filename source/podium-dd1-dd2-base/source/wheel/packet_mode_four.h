#ifndef OPENTEC_BASE_WHEEL_PACKET_MODE_FOUR_H
#define OPENTEC_BASE_WHEEL_PACKET_MODE_FOUR_H

#include <stdint.h>

#include "wheel/display_output.h"

/** @brief Mode-four packet dimensions and field counts. */
enum {
    WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE = 9,        /**< Response size in bytes. */
    WHEEL_PACKET_MODE_FOUR_REQUEST_SIZE = 32,        /**< Request size in bytes. */
    WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE = 30,       /**< Snapshot size in bytes. */
    WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT = 3,         /**< Number of button bytes. */
    WHEEL_PACKET_MODE_FOUR_BUTTON_HISTORY_DEPTH = 3, /**< Button-history sample count. */
    WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT = 4,        /**< Number of control bytes. */
    WHEEL_PACKET_MODE_FOUR_AXIS_OUTPUT_COUNT = 2,    /**< Number of axis-output bytes. */
    WHEEL_PACKET_MODE_FOUR_AXIS_VALUE_COUNT = 2,     /**< Number of 16-bit axis values. */
    WHEEL_PACKET_MODE_FOUR_CONTROL_DATA_COUNT = 4,   /**< Number of control-data bytes. */
    WHEEL_PACKET_MODE_FOUR_AUXILIARY_DATA_COUNT = 4, /**< Number of auxiliary-data bytes. */
};

/** @brief Logical input fields decoded from a mode-four request. */
typedef struct {
    uint8_t buttons[WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT];            /**< Button bytes. */
    uint8_t axis_outputs[WHEEL_PACKET_MODE_FOUR_AXIS_OUTPUT_COUNT];  /**< Axis-output bytes. */
    int8_t motion;                                                   /**< Primary motion flags. */
    uint8_t controls[WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT];          /**< Control bytes. */
    uint8_t control_data[WHEEL_PACKET_MODE_FOUR_CONTROL_DATA_COUNT]; /**< Control-data bytes. */
    uint8_t reserved_axes[2];                                        /**< Reserved axis bytes. */
    uint16_t axis_values[WHEEL_PACKET_MODE_FOUR_AXIS_VALUE_COUNT];   /**< 16-bit axis values. */
    uint8_t mode_buttons;        /**< Secondary mode-button byte. */
    uint8_t axis_report_enabled; /**< Axis-report enable flag. */
    uint8_t auxiliary_data[WHEEL_PACKET_MODE_FOUR_AUXILIARY_DATA_COUNT]; /**< Auxiliary data. */
    uint8_t report_mode;         /**< Report-mode selector. */
    uint8_t reserved_report;     /**< Reserved report byte. */
    uint8_t report_capabilities; /**< Report capability flags. */
    uint8_t axis_limit;          /**< Axis-limit value. */
} WheelPacketModeFourInput;

/** @brief Button and control histories for mode-four filtering. */
typedef struct {
    uint8_t button_samples[WHEEL_PACKET_MODE_FOUR_BUTTON_HISTORY_DEPTH]
                          [WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT]; /**< Recent button samples. */
    uint8_t next_button_sample; /**< History index receiving the next button sample. */
} WheelPacketModeFourFilter;

/** @brief Retained mode-four extension and axis-report latches. */
typedef struct {
    uint8_t extended_buttons;    /**< Latched extended button bits. */
    uint8_t axis_report_enabled; /**< Latched axis-report enable state. */
} WheelPacketModeFourRuntime;

/** @brief Display, vibration, and axis fields for a mode-four response. */
typedef struct {
    WheelDisplayOutput display; /**< Display output. */
    uint8_t vibration[2];       /**< Two vibration-channel values. */
    uint8_t legacy_axes[2];     /**< Two legacy output-axis values. */
} WheelPacketModeFourOutput;

/**
 * @brief Initializes mode-four input filtering.
 *
 * Clears button and control histories and resets both circular insertion indices.
 *
 * @param[out] filter Mode-four filter state to initialize.
 */
void wheel_packet_mode_four_filter_init(WheelPacketModeFourFilter *filter);

/**
 * @brief Decodes a mode-four attached-wheel request.
 *
 * Copies the request payload into the logical mode-four input fields.
 *
 * @param[in] request Thirty-two-byte mode-four request.
 * @param[out] input Mode-four input state to populate.
 */
void wheel_packet_mode_four_decode(const uint8_t request[WHEEL_PACKET_MODE_FOUR_REQUEST_SIZE],
                                   WheelPacketModeFourInput *input);

/**
 * @brief Filters one mode-four input sample.
 *
 * Retains button bits across three samples and control bits across four samples, then updates the
 * input in place.
 *
 * @param[in,out] filter Button and control histories to update.
 * @param[in,out] input Input sample to filter in place.
 */
void wheel_packet_mode_four_filter(WheelPacketModeFourFilter *filter,
                                   WheelPacketModeFourInput *input);

/**
 * @brief Normalizes filtered mode-four input.
 *
 * Applies interface-specific mappings, latches axis-report state, masks unsupported controls, and
 * writes the normalized snapshot.
 *
 * @param[in,out] input Filtered mode-four input to normalize.
 * @param[in] interface_mode Active host interface mode.
 * @param[in,out] runtime Persistent mode-four runtime latches.
 * @param[out] snapshot Thirty-byte normalized snapshot destination.
 */
void wheel_packet_mode_four_normalize(WheelPacketModeFourInput *input, uint8_t interface_mode,
                                      WheelPacketModeFourRuntime *runtime,
                                      uint8_t snapshot[WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE]);

/**
 * @brief Encodes a mode-four attached-wheel response.
 *
 * Writes the select-mode command, display output, vibration channels, and legacy axes.
 *
 * @param[in] output Mode-four response output state.
 * @param[out] response Nine-byte response destination.
 */
void wheel_packet_mode_four_encode(const WheelPacketModeFourOutput *output,
                                   uint8_t response[WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE]);

#endif
