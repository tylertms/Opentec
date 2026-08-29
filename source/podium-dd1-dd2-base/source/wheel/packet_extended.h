#ifndef OPENTEC_BASE_WHEEL_PACKET_EXTENDED_H
#define OPENTEC_BASE_WHEEL_PACKET_EXTENDED_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_EXTENDED_MODE_STANDARD = 0x0a,
    WHEEL_PACKET_EXTENDED_MODE_STATUS = 0x1b,
    WHEEL_PACKET_EXTENDED_PULSE_PAIR_COUNT = 4,
};

/** @brief Logical input carried by the extended attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketExtendedInput;

/** @brief Retained direct-interface pulse flags and their independent expiry times. */
typedef struct {
    uint32_t deadlines_ms[WHEEL_PACKET_EXTENDED_PULSE_PAIR_COUNT];
    uint8_t active_flags;
} WheelPacketExtendedPulseState;

bool wheel_packet_extended_applies(uint8_t wheel_mode);
int8_t wheel_packet_extended_primary_delta(const WheelPacketExtendedInput *input);
void wheel_packet_extended_swap_buttons(WheelPacketExtendedInput *input);
void wheel_packet_extended_pulse_init(WheelPacketExtendedPulseState *state);
uint8_t wheel_packet_extended_hold_direct_pulses(WheelPacketExtendedPulseState *state,
                                                 uint8_t wheel_mode, uint32_t now_ms,
                                                 uint8_t flags);

#endif
