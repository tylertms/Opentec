#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

static void test_selects_and_encodes_each_scan_phase(void) {
    const WheelDisplayOutput output = {
        .glyphs = {0xa5, 0x5a, 0x40},
        .auxiliary = 0x37,
        .third_glyph_marker = false,
    };

    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_FIRST) == 0xc9);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_SECOND) == 0x36);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_THIRD) == 0x02);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_AUXILIARY) == 0x37);
}

static void test_adds_the_phase_four_marker(void) {
    const WheelDisplayOutput output = {
        .glyphs = {0, 0, 0x40},
        .third_glyph_marker = true,
    };

    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_THIRD) == 0x0a);
}

static void test_prioritizes_lower_scan_phase_bits(void) {
    const WheelDisplayOutput output = {
        .glyphs = {0x01, 0x02, 0x04},
        .auxiliary = 0xff,
        .third_glyph_marker = true,
    };

    assert(wheel_display_output_encode(&output, 0) == 0);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_FIRST | WHEEL_SCAN_PHASE_SECOND) ==
           0x40);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_FIRST | WHEEL_SCAN_PHASE_THIRD) ==
           0x40);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_SECOND | WHEEL_SCAN_PHASE_THIRD) ==
           0x10);
    assert(wheel_display_output_encode(&output, WHEEL_SCAN_PHASE_THIRD |
                                                    WHEEL_SCAN_PHASE_AUXILIARY) == 0x09);
}

static void test_decodes_protocol_characters(void) {
    static const uint8_t glyphs[] = {0x3f, 0x06, 0x30, 0x5b, 0x4f, 0x66, 0x6d, 0x7d,
                                     0x07, 0x27, 0x7f, 0x6f, 0x39, 0x0f, 0x50, 0x54};
    static const uint8_t characters[] = {'0', '1', '1', '2', '3', '4', '5', '6',
                                         '7', '7', '8', '9', '(', ')', 'R', 'N'};
    for (uint8_t index = 0; index < sizeof(glyphs); index++) {
        assert(wheel_display_output_character(glyphs[index]) == characters[index]);
        assert(wheel_display_output_character(glyphs[index] | 0x80U) == characters[index]);
    }
    assert(wheel_display_output_character(0) == ' ');
}

int main(void) {
    test_selects_and_encodes_each_scan_phase();
    test_adds_the_phase_four_marker();
    test_prioritizes_lower_scan_phase_bits();
    test_decodes_protocol_characters();
    return 0;
}
