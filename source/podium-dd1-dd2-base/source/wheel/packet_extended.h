#ifndef OPENTEC_BASE_WHEEL_PACKET_EXTENDED_H
#define OPENTEC_BASE_WHEEL_PACKET_EXTENDED_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

/** @brief Extended-packet mode values and pulse-pair dimensions. */
enum {
    WHEEL_PACKET_EXTENDED_MODE_STANDARD = 0x0a, /**< Standard extended-packet mode. */
    WHEEL_PACKET_EXTENDED_MODE_STATUS = 0x1b,   /**< Extended status-packet mode. */
    WHEEL_PACKET_EXTENDED_MODE_REMOTE = 0x1c,   /**< Remote-capable extended-packet mode. */
    WHEEL_PACKET_EXTENDED_PULSE_PAIR_COUNT = 4, /**< Number of retained pulse pairs. */
};

/** @brief Logical input carried by the extended attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketExtendedInput;

/** @brief Retained direct-interface pulse flags and their independent expiry times. */
typedef struct {
    uint32_t deadlines_ms[WHEEL_PACKET_EXTENDED_PULSE_PAIR_COUNT]; /**< Expiry deadline per pulse
                                                                      pair. */
    uint8_t active_flags; /**< Currently retained pulse flags. */
} WheelPacketExtendedPulseState;

/**
 * @brief Reports whether a wheel mode uses extended packets.
 *
 * Selects modes 0x0A, 0x1B, and 0x1C.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for modes 0x0A, 0x1B, and 0x1C; otherwise false.
 */
bool wheel_packet_extended_applies(uint8_t wheel_mode);

/**
 * @brief Decodes the primary rotary step from extended input.
 *
 * Reads the packet's opposing primary direction flags and returns their signed step.
 *
 * @param[in] input Decoded extended-packet input.
 * @return Positive one, negative one, or zero when no step is present.
 */
int8_t wheel_packet_extended_primary_delta(const WheelPacketExtendedInput *input);

/**
 * @brief Swaps the extended packet's designated button bits.
 *
 * Exchanges button bit eight with button bit eleven in place.
 *
 * @param[in,out] input Extended-packet input to update.
 */
void wheel_packet_extended_swap_buttons(WheelPacketExtendedInput *input);

/**
 * @brief Initializes retained extended pulse state.
 *
 * Clears pulse deadlines and all active pulse flags.
 *
 * @param[out] state Pulse state to initialize.
 */
void wheel_packet_extended_pulse_init(WheelPacketExtendedPulseState *state);

/**
 * @brief Retains direct-interface extended pulse pairs.
 *
 * Resolves opposing flags, expires old pairs, and retains accepted direct-interface pulses until
 * their independent deadlines.
 *
 * @param[in,out] state Pulse flags and deadlines to update.
 * @param[in] wheel_mode Active extended-packet mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] flags Current packet pulse flags.
 * @return Active pulse flags after conflict resolution and expiry.
 */
uint8_t wheel_packet_extended_hold_direct_pulses(WheelPacketExtendedPulseState *state,
                                                 uint8_t wheel_mode, uint32_t now_ms,
                                                 uint8_t flags);

#endif
