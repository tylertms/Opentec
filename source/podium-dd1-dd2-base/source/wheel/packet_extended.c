#include "wheel/packet_extended.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal extended-packet masks and direct-pulse timing. */
enum {
    PRIMARY_POSITIVE_PULSE = 0x10, /**< Primary positive-direction pulse flag. */
    PRIMARY_NEGATIVE_PULSE = 0x20, /**< Primary negative-direction pulse flag. */
    FIRST_REMAPPED_BUTTON = 0x01,  /**< First compatibility button mask. */
    SECOND_REMAPPED_BUTTON = 0x08, /**< Second compatibility button mask. */
    DIRECT_PULSE_HOLD_MS = 80,     /**< Direct pulse retention interval in milliseconds. */
};

/**
 * @brief Tests whether a retained pulse deadline has elapsed.
 *
 * Uses signed subtraction so the comparison remains valid when the millisecond counter wraps.
 *
 * @param[in] now_ms Current monotonic millisecond count.
 * @param[in] deadline_ms Retained pulse deadline.
 * @return True at and after the deadline.
 */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief Resolves simultaneous directions in one retained pulse pair.
 *
 * Replaces a contradictory retained pair with the directions carried by the current packet.
 *
 * @param[in,out] active_flags Retained pulse flags.
 * @param[in] current_flags Current packet pulse flags.
 * @param[in] pair_mask Two-bit directional pair mask.
 */
static void resolve_pair(uint8_t *active_flags, uint8_t current_flags, uint8_t pair_mask) {
    if ((*active_flags & pair_mask) == pair_mask) {
        *active_flags = (*active_flags & (uint8_t)~pair_mask) | (current_flags & pair_mask);
    }
}

/**
 * @brief Reports whether a wheel mode uses the extended packet policy.
 *
 * Selects the authenticated mode 0x0A and status mode 0x1B variants that share the common payload
 * and input processing behavior.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x0A or mode 0x1B.
 */
bool wheel_packet_extended_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_EXTENDED_MODE_STANDARD ||
           wheel_mode == WHEEL_PACKET_EXTENDED_MODE_STATUS;
}

/**
 * @brief Decodes the extended packet's primary rotary step.
 *
 * Gives the positive flag priority, otherwise selects the negative flag or idle.
 *
 * @param[in] input Decoded extended packet.
 * @return Positive one, negative one, or zero.
 */
int8_t wheel_packet_extended_primary_delta(const WheelPacketExtendedInput *input) {
    uint8_t flags = (uint8_t)input->motion;
    if ((flags & PRIMARY_POSITIVE_PULSE) != 0) {
        return 1;
    }
    return (flags & PRIMARY_NEGATIVE_PULSE) != 0 ? -1 : 0;
}

/**
 * @brief Exchanges extended packet button bits eight and eleven.
 *
 * Preserves every other button while moving each compatibility bit into the other's position.
 *
 * @param[in,out] input Filtered extended input updated in place.
 */
void wheel_packet_extended_swap_buttons(WheelPacketExtendedInput *input) {
    uint8_t buttons = input->buttons[1];
    uint8_t first = buttons & FIRST_REMAPPED_BUTTON;
    uint8_t second = buttons & SECOND_REMAPPED_BUTTON;
    buttons &= (uint8_t)~(FIRST_REMAPPED_BUTTON | SECOND_REMAPPED_BUTTON);
    input->buttons[1] = buttons | (first != 0 ? SECOND_REMAPPED_BUTTON : 0) |
                        (second != 0 ? FIRST_REMAPPED_BUTTON : 0);
}

/**
 * @brief Clears retained direct-interface pulse state.
 *
 * Resets all four directional-pair deadlines and removes every retained pulse flag.
 *
 * @param[out] state Direct-interface pulse state to initialize.
 */
void wheel_packet_extended_pulse_init(WheelPacketExtendedPulseState *state) {
    for (uint8_t pair = 0; pair < WHEEL_PACKET_EXTENDED_PULSE_PAIR_COUNT; pair++) {
        state->deadlines_ms[pair] = 0;
    }
    state->active_flags = 0;
}

/**
 * @brief Retains direct-interface extended pulse pairs.
 *
 * Merges current directions into the retained pulse set, resolves contradictory pairs from the
 * current packet, and holds each active pair for 80 milliseconds. Mode 0x0A uses three pairs;
 * mode 0x1B additionally retains bits six and seven.
 *
 * @param[in,out] state Retained pulse flags and independent pair deadlines.
 * @param[in] wheel_mode Active extended packet mode.
 * @param[in] now_ms Current monotonic millisecond count.
 * @param[in] flags Current packet pulse flags.
 * @return Retained pulse flags after conflict resolution and expiry.
 */
uint8_t wheel_packet_extended_hold_direct_pulses(WheelPacketExtendedPulseState *state,
                                                 uint8_t wheel_mode, uint32_t now_ms,
                                                 uint8_t flags) {
    uint8_t pair_count = wheel_mode == WHEEL_PACKET_EXTENDED_MODE_STATUS ? 4 : 3;
    uint8_t tracked_mask = pair_count == 4 ? UINT8_MAX : 0x3fu;
    state->active_flags = (state->active_flags | flags) & tracked_mask;

    for (uint8_t pair = 0; pair < pair_count; pair++) {
        uint8_t pair_mask = (uint8_t)(3u << (pair * 2u));
        resolve_pair(&state->active_flags, flags, pair_mask);
        if ((flags & pair_mask) != 0) {
            state->deadlines_ms[pair] = now_ms + DIRECT_PULSE_HOLD_MS;
        }
        if (deadline_reached(now_ms, state->deadlines_ms[pair])) {
            state->active_flags &= (uint8_t)~pair_mask;
        }
    }
    return state->active_flags;
}
