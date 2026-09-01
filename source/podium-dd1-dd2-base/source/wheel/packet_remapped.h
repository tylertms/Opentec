#ifndef OPENTEC_BASE_WHEEL_PACKET_REMAPPED_H
#define OPENTEC_BASE_WHEEL_PACKET_REMAPPED_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

/** @brief Remapped-packet filter dimensions. */
enum {
    WHEEL_PACKET_REMAPPED_HISTORY_DEPTH = 3, /**< Button-history sample count. */
};

/** @brief Logical input carried by the remapped attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketRemappedInput;

/** @brief Three-sample button history for the remapped packet family. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_REMAPPED_HISTORY_DEPTH]
                   [WHEEL_PACKET_COMMON_BUTTON_COUNT]; /**< Recent button samples. */
    uint8_t next_sample;                               /**< Index receiving the next sample. */
} WheelPacketRemappedFilter;

/**
 * @brief Reports whether a wheel mode uses remapped packets.
 *
 * Selects operating mode 0x11.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for mode 0x11; otherwise false.
 */
bool wheel_packet_remapped_applies(uint8_t wheel_mode);

/**
 * @brief Initializes remapped-packet button filtering.
 *
 * Clears retained button samples and resets the insertion index.
 *
 * @param[out] filter Filter state to initialize.
 */
void wheel_packet_remapped_filter_init(WheelPacketRemappedFilter *filter);

/**
 * @brief Remaps and filters one remapped-packet input.
 *
 * Applies interface-specific button mapping, retains bits present across recent samples, and
 * updates the input in place.
 *
 * @param[in,out] filter Button-history state to update.
 * @param[in,out] input Input to remap and filter in place.
 * @param[in] interface_mode Active host interface mode.
 */
void wheel_packet_remapped_filter(WheelPacketRemappedFilter *filter,
                                  WheelPacketRemappedInput *input, uint8_t interface_mode);

/**
 * @brief Decodes the remapped packet's primary motion step.
 *
 * Reads opposing primary direction flags and returns their signed step.
 *
 * @param[in] input Remapped-packet input containing motion flags.
 * @return Positive one, negative one, or zero when no step is present.
 */
int8_t wheel_packet_remapped_primary_delta(const WheelPacketRemappedInput *input);

#endif
