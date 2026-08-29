#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_extended.h"

static void test_selects_extended_packet_family(void) {
    for (uint16_t mode = 0; mode <= UINT8_MAX; mode++) {
        bool expected = mode == WHEEL_PACKET_EXTENDED_MODE_STANDARD ||
                        mode == WHEEL_PACKET_EXTENDED_MODE_STATUS;
        assert(wheel_packet_extended_applies((uint8_t)mode) == expected);
    }
}

static void test_decodes_primary_pulse_priority(void) {
    WheelPacketExtendedInput input = {0};
    assert(wheel_packet_extended_primary_delta(&input) == 0);
    input.motion = 0x20;
    assert(wheel_packet_extended_primary_delta(&input) == -1);
    input.motion = 0x10;
    assert(wheel_packet_extended_primary_delta(&input) == 1);
    input.motion = 0x30;
    assert(wheel_packet_extended_primary_delta(&input) == 1);
}

static void test_swaps_compatibility_buttons(void) {
    WheelPacketExtendedInput input = {0};
    input.buttons[1] = 0xa5;
    wheel_packet_extended_swap_buttons(&input);
    assert(input.buttons[1] == 0xac);

    input.buttons[1] = 0x08;
    wheel_packet_extended_swap_buttons(&input);
    assert(input.buttons[1] == 0x01);

    input.buttons[1] = 0x09;
    wheel_packet_extended_swap_buttons(&input);
    assert(input.buttons[1] == 0x09);
}

static void test_latches_common_packet_buttons(void) {
    WheelPacketExtendedInput input = {0};
    input.buttons[1] = 0x09;
    wheel_packet_common_latch_buttons(&input, true, false);
    assert(input.controls[3] == 0x03);

    input.buttons[1] = 0;
    wheel_packet_common_latch_buttons(&input, true, false);
    assert(input.buttons[1] == 0x09);

    input.buttons[1] = 0;
    wheel_packet_common_latch_buttons(&input, true, true);
    assert(input.buttons[1] == 0);
}

static void test_holds_direct_pulse_pairs_independently(void) {
    WheelPacketExtendedPulseState state;
    wheel_packet_extended_pulse_init(&state);

    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STATUS, 10,
                                                    0x51) == 0x51);
    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STATUS, 40,
                                                    0) == 0x51);
    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STATUS, 90,
                                                    0) == 0);
}

static void test_resolves_conflicting_directions_from_current_packet(void) {
    WheelPacketExtendedPulseState state;
    wheel_packet_extended_pulse_init(&state);

    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STATUS, 1,
                                                    0x01) == 0x01);
    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STATUS, 2,
                                                    0x02) == 0x02);
    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STATUS, 3,
                                                    0x03) == 0x03);
}

static void test_ignores_status_pair_in_standard_mode(void) {
    WheelPacketExtendedPulseState state;
    wheel_packet_extended_pulse_init(&state);

    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STANDARD, 1,
                                                    0xc0) == 0);
    assert(wheel_packet_extended_hold_direct_pulses(&state, WHEEL_PACKET_EXTENDED_MODE_STATUS, 2,
                                                    0x40) == 0x40);
}

int main(void) {
    test_selects_extended_packet_family();
    test_decodes_primary_pulse_priority();
    test_swaps_compatibility_buttons();
    test_latches_common_packet_buttons();
    test_holds_direct_pulse_pairs_independently();
    test_resolves_conflicting_directions_from_current_packet();
    test_ignores_status_pair_in_standard_mode();
    return 0;
}
