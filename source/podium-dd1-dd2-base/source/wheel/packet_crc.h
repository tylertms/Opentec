#ifndef OPENTEC_BASE_WHEEL_PACKET_CRC_H
#define OPENTEC_BASE_WHEEL_PACKET_CRC_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/adapter.h"
#include "wheel/display_output.h"
#include "wheel/packet_common.h"

/** @brief CRC-packet dimensions and history sizes. */
enum {
    WHEEL_PACKET_CRC_RESPONSE_SIZE = 33,         /**< Complete response size in bytes. */
    WHEEL_PACKET_CRC_CONTENT_SIZE =
        WHEEL_PACKET_CRC_RESPONSE_SIZE - 1, /**< Response bytes covered by the CRC. */
    WHEEL_PACKET_CRC_REQUEST_SIZE = 33,          /**< Complete request size in bytes. */
    WHEEL_PACKET_CRC_SNAPSHOT_SIZE = 31, /**< Processed payload plus the received CRC byte. */
    WHEEL_PACKET_CRC_BUTTON_COUNT = 3,           /**< Number of filtered button bytes. */
    WHEEL_PACKET_CRC_FILTERED_CONTROL_COUNT = 5, /**< Number of filtered control bytes. */
    WHEEL_PACKET_CRC_CONTROL_COUNT = 8,          /**< Number of logical control bytes. */
    WHEEL_PACKET_CRC_HISTORY_DEPTH = 3,          /**< Number of retained filter samples. */
    WHEEL_PACKET_CRC_AXIS_VALUE_COUNT = 2,       /**< Number of 16-bit axis values. */
};

/** @brief Logical input carried by the CRC attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketCrcInput;

/** @brief Button, control, and axis histories for CRC-packet filtering. */
typedef struct {
    uint8_t button_samples[WHEEL_PACKET_CRC_HISTORY_DEPTH]
                          [WHEEL_PACKET_CRC_BUTTON_COUNT]; /**< Recent button samples. */
    uint8_t
        control_samples[WHEEL_PACKET_CRC_HISTORY_DEPTH]
                       [WHEEL_PACKET_CRC_FILTERED_CONTROL_COUNT]; /**< Recent control samples. */
    uint8_t axis_samples[WHEEL_PACKET_CRC_HISTORY_DEPTH]
                        [WHEEL_PACKET_CRC_AXIS_VALUE_COUNT]; /**< Recent axis samples. */
    uint8_t next_button_sample;  /**< Index receiving the next button sample. */
    uint8_t next_control_sample; /**< Index receiving the next control sample. */
    uint8_t next_axis_sample;    /**< Index receiving the next axis sample. */
} WheelPacketCrcFilter;

/** @brief Display, axis, and status state for CRC-packet responses. */
typedef struct {
    WheelDisplayOutput display; /**< Current display output. */
    uint8_t legacy_axes[2];     /**< Two legacy output-axis values. */
    uint8_t report_state;        /**< Current attached-wheel report state. */
    bool status_update_pending;  /**< Whether report status needs publication. */
} WheelPacketCrcOutput;

/**
 * @brief Reports whether a wheel mode uses CRC-packet processing.
 *
 * Selects CRC mode 0x06 and authenticated CRC mode 0x15.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for mode 0x06 or 0x15; otherwise false.
 */
bool wheel_packet_crc_applies(uint8_t wheel_mode);

/**
 * @brief Initializes CRC-packet filter histories.
 *
 * Clears button, control, and axis samples and resets all insertion indices.
 *
 * @param[out] filter CRC filter state to initialize.
 */
void wheel_packet_crc_filter_init(WheelPacketCrcFilter *filter);

/**
 * @brief Decodes a CRC-family attached-wheel request.
 *
 * Copies the request payload fields into a logical CRC-family input while ignoring the CRC byte.
 *
 * @param[in] request Thirty-three-byte request containing payload and CRC.
 * @param[out] input Logical CRC-family input to populate.
 */
void wheel_packet_crc_decode(const uint8_t request[WHEEL_PACKET_CRC_REQUEST_SIZE],
                             WheelPacketCrcInput *input);

/**
 * @brief Prepares CRC-family input before filtering.
 *
 * Applies mode- and interface-specific remapping to the decoded input.
 *
 * @param[in,out] input Decoded CRC-family input to update.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] interface_mode Active host interface mode.
 */
void wheel_packet_crc_prepare(WheelPacketCrcInput *input, uint8_t wheel_mode,
                              uint8_t interface_mode);

/**
 * @brief Filters CRC-family buttons and controls.
 *
 * Retains bits present across the recent button and control samples and advances their histories.
 *
 * @param[in,out] filter Button and control histories to update.
 * @param[in,out] input Input sample to filter in place.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 */
void wheel_packet_crc_filter(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input,
                             uint8_t wheel_mode);

/**
 * @brief Smooths CRC-family auxiliary axes.
 *
 * Retains recent axis values and averages each pair into the input.
 *
 * @param[in,out] filter Axis histories to update.
 * @param[in,out] input Input whose axis values are smoothed in place.
 */
void wheel_packet_crc_smooth_axes(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input);

/**
 * @brief Normalizes CRC-family wheel and adapter input.
 *
 * Maps controls and adapter state to the active interface representation and consumes queued
 * adapter motion when supplied.
 *
 * @param[in,out] input Filtered CRC-family input to normalize.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] interface_mode Active host interface mode.
 * @param[in,out] adapter Adapter state to merge, or null for direct-wheel input.
 */
void wheel_packet_crc_normalize(WheelPacketCrcInput *input, uint8_t wheel_mode,
                                uint8_t interface_mode, WheelAdapterInput *adapter);

/**
 * @brief Serializes normalized CRC-family input.
 *
 * Writes the logical input fields into the first thirty snapshot bytes and appends the received
 * request CRC. The official processor copies all thirty-one bytes after normalizing the payload.
 *
 * @param[in] input Normalized CRC-family input.
 * @param[in] request_crc CRC byte received with the request.
 * @param[out] snapshot Thirty-one-byte processed-request snapshot destination.
 */
void wheel_packet_crc_snapshot(const WheelPacketCrcInput *input, uint8_t request_crc,
                               uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE]);

/**
 * @brief Encodes a CRC-family attached-wheel response.
 *
 * Writes display, legacy-axis, report-state, and one-shot status fields. Existing display-value and
 * reserved-tail bytes remain unchanged, and the final byte receives the CRC-8 over bytes zero
 * through thirty-one.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in,out] output Response state to encode and update.
 * @param[out] response Thirty-three-byte response destination.
 */
void wheel_packet_crc_encode(uint8_t wheel_mode, WheelPacketCrcOutput *output,
                             uint8_t response[WHEEL_PACKET_CRC_RESPONSE_SIZE]);

#endif
