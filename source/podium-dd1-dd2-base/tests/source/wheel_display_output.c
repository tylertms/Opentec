#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

static void test_selects_and_encodes_each_scan_phase(void) {
    const WheelDisplayOutput output = {
        .glyphs = {0xa5, 0x5a, 0x40},
        .auxiliary = 0x37,
        .phase_four_marker = false,
    };

    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_FIRST) == 0xc9);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_SECOND) == 0x36);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_THIRD) == 0x02);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_AUXILIARY) == 0x37);
}

static void test_adds_the_phase_four_marker(void) {
    const WheelDisplayOutput output = {
        .glyphs = {0, 0, 0x40},
        .phase_four_marker = true,
    };

    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_THIRD) == 0x0a);
}

static void test_prioritizes_lower_scan_phase_bits(void) {
    const WheelDisplayOutput output = {
        .glyphs = {0x01, 0x02, 0x04},
        .auxiliary = 0xff,
        .phase_four_marker = true,
    };

    assert(wheel_display_output_encode(&output, 0) == 0);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_FIRST | WHEEL_SCAN_PHASE_SECOND) ==
           0x40);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_THIRD |
                                                    WHEEL_SCAN_PHASE_AUXILIARY) == 0x09);
}

int main(void) {
    test_selects_and_encodes_each_scan_phase();
    test_adds_the_phase_four_marker();
    test_prioritizes_lower_scan_phase_bits();
    return 0;
}
