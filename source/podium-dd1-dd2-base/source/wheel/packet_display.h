#ifndef OPENTEC_BASE_WHEEL_PACKET_DISPLAY_H
#define OPENTEC_BASE_WHEEL_PACKET_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"
#include "wheel/packet_common.h"

/** @brief Standard display-packet dimensions and filter sizes. */
enum {
    WHEEL_PACKET_DISPLAY_REQUEST_SIZE =
        WHEEL_PACKET_COMMON_REQUEST_SIZE, /**< Request size in bytes. */
    WHEEL_PACKET_DISPLAY_RESPONSE_SIZE =
        WHEEL_PACKET_COMMON_RESPONSE_SIZE, /**< Response size in bytes. */
    WHEEL_PACKET_DISPLAY_SNAPSHOT_SIZE =
        WHEEL_PACKET_COMMON_SNAPSHOT_SIZE,  /**< Snapshot size in bytes. */
    WHEEL_PACKET_DISPLAY_FILTER_WIDTH = 6,  /**< Button and leading-control fields filtered. */
    WHEEL_PACKET_DISPLAY_HISTORY_DEPTH = 3, /**< Number of retained filter samples. */
};

/** @brief Logical input carried by the standard display packet family. */
typedef WheelPacketCommonInput WheelPacketDisplayInput;

/** @brief Shared three-sample history for display-packet buttons and leading controls. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_DISPLAY_HISTORY_DEPTH]
                   [WHEEL_PACKET_DISPLAY_FILTER_WIDTH]; /**< Recent button and control samples. */
    uint8_t next_sample;                                /**< Index receiving the next sample. */
} WheelPacketDisplayFilter;

/**
 * @brief Reports whether a wheel mode uses standard display packets.
 *
 * Selects operating mode 0x10.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for mode 0x10; otherwise false.
 */
bool wheel_packet_display_applies(uint8_t wheel_mode);

/**
 * @brief Initializes standard display-packet filtering.
 *
 * Clears retained button and control samples and resets the insertion index.
 *
 * @param[out] filter Filter state to initialize.
 */
void wheel_packet_display_filter_init(WheelPacketDisplayFilter *filter);

/**
 * @brief Filters one standard display-packet input.
 *
 * Retains button bits and leading control bits present across recent samples and updates the input
 * in place.
 *
 * @param[in,out] filter Filter history to update.
 * @param[in,out] input Input sample to filter in place.
 */
void wheel_packet_display_filter(WheelPacketDisplayFilter *filter, WheelPacketDisplayInput *input);

#endif
