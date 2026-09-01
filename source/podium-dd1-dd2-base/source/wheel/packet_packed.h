#ifndef OPENTEC_BASE_WHEEL_PACKET_PACKED_H
#define OPENTEC_BASE_WHEEL_PACKET_PACKED_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"
#include "wheel/packet_common.h"

/** @brief Packed-packet dimensions and field counts. */
enum {
    WHEEL_PACKET_PACKED_REQUEST_SIZE = 32,    /**< Request size in bytes. */
    WHEEL_PACKET_PACKED_RESPONSE_SIZE = 9,    /**< Response size in bytes. */
    WHEEL_PACKET_PACKED_SNAPSHOT_SIZE = 30,   /**< Snapshot size in bytes. */
    WHEEL_PACKET_PACKED_BUTTON_COUNT = 3,     /**< Number of button bytes. */
    WHEEL_PACKET_PACKED_CONTROL_COUNT = 8,    /**< Number of control bytes. */
    WHEEL_PACKET_PACKED_AXIS_VALUE_COUNT = 2, /**< Number of 16-bit axis values. */
    WHEEL_PACKET_PACKED_HISTORY_DEPTH = 3,    /**< Number of retained button samples. */
};

/** @brief Logical input carried by the packed attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketPackedInput;

/** @brief Three-sample button history for the packed packet family. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_PACKED_HISTORY_DEPTH]
                   [WHEEL_PACKET_PACKED_BUTTON_COUNT];          /**< Recent button samples. */
    uint8_t axis_samples[WHEEL_PACKET_PACKED_HISTORY_DEPTH][2]; /**< Recent packed axis samples. */
    uint8_t next_sample; /**< Index receiving the next sample. */
} WheelPacketPackedFilter;

/**
 * @brief Reports whether a wheel mode uses packed packets.
 *
 * Selects the legacy-alternate and legacy-compatibility modes handled by this codec.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for modes 0x0F and 0x17; otherwise false.
 */
bool wheel_packet_packed_applies(uint8_t wheel_mode);

/**
 * @brief Initializes packed-packet button filtering.
 *
 * Clears retained button samples and resets the insertion index.
 *
 * @param[out] filter Button filter state to initialize.
 */
void wheel_packet_packed_filter_init(WheelPacketPackedFilter *filter);

/**
 * @brief Filters one packed-packet button sample.
 *
 * Retains button bits present across all recent samples and updates the input in place.
 *
 * @param[in,out] filter Button history to update.
 * @param[in,out] input Input sample to filter in place.
 */
void wheel_packet_packed_filter_buttons(WheelPacketPackedFilter *filter,
                                        WheelPacketPackedInput *input);

/**
 * @brief Decodes a packed attached-wheel request.
 *
 * Copies the request payload into the common logical input fields.
 *
 * @param[in] request Thirty-two-byte packed request.
 * @param[out] input Packed input state to populate.
 */
void wheel_packet_packed_decode(const uint8_t request[WHEEL_PACKET_PACKED_REQUEST_SIZE],
                                WheelPacketPackedInput *input);

/**
 * @brief Normalizes packed-packet input.
 *
 * Swaps the designated button bits and expands the packed axis word into the first two controls.
 *
 * @param[in,out] input Packed input to normalize.
 */
void wheel_packet_packed_normalize(WheelPacketPackedInput *input);

/**
 * @brief Serializes normalized packed-packet input.
 *
 * Writes the logical input fields into the thirty-byte snapshot layout.
 *
 * @param[in] input Normalized packed input.
 * @param[out] snapshot Thirty-byte snapshot destination.
 */
void wheel_packet_packed_snapshot(const WheelPacketPackedInput *input,
                                  uint8_t snapshot[WHEEL_PACKET_PACKED_SNAPSHOT_SIZE]);

/**
 * @brief Encodes a packed attached-wheel response.
 *
 * Writes the authenticated command, display output, vibration channels, and legacy axes.
 *
 * @param[in] display Display output.
 * @param[in] vibration Two vibration-channel values.
 * @param[in] legacy_axes Two legacy output-axis values.
 * @param[out] response Nine-byte response destination.
 */
void wheel_packet_packed_encode(const WheelDisplayOutput *display, const uint8_t vibration[2],
                                const uint8_t legacy_axes[2],
                                uint8_t response[WHEEL_PACKET_PACKED_RESPONSE_SIZE]);

#endif
