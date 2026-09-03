#include "wheel/display_output.h"

#include <stdint.h>

/**
 * @brief Encodes the active display output for an attached-wheel scan phase.
 *
 * Selects one of three display glyphs or the auxiliary byte, permutes glyph segments into scan
 * order, and adds the phase-four marker for a third-only scan when requested. Lower phase bits take
 * priority.
 *
 * @param[in] output Three display glyphs, the auxiliary byte, and the phase-four marker state.
 * @param[in] phase Scan phase bit field.
 * @return Permuted glyph byte, auxiliary byte, or zero when no scan phase is selected.
 */
uint8_t wheel_display_output_encode(const WheelDisplayOutput *output, uint8_t phase) {
    if ((phase & WHEEL_SCAN_PHASE_AUXILIARY) != 0 &&
        (phase & (WHEEL_SCAN_PHASE_FIRST | WHEEL_SCAN_PHASE_SECOND | WHEEL_SCAN_PHASE_THIRD)) ==
            0) {
        return output->auxiliary;
    }

    uint8_t glyph;
    if ((phase & WHEEL_SCAN_PHASE_FIRST) != 0) {
        glyph = output->glyphs[0];
    } else if ((phase & WHEEL_SCAN_PHASE_SECOND) != 0) {
        glyph = output->glyphs[1];
    } else if ((phase & WHEEL_SCAN_PHASE_THIRD) != 0) {
        glyph = output->glyphs[2];
    } else {
        return 0;
    }

    uint8_t encoded =
        (uint8_t)(((glyph & 0x01u) << 6) | ((glyph & 0x02u) << 3) | ((glyph & 0x04u) >> 2) |
                  ((glyph & 0x08u) >> 1) | ((glyph & 0x10u) << 1) | ((glyph & 0x20u) << 2) |
                  ((glyph & 0x40u) >> 5) | ((glyph & 0x80u) >> 4));
    if ((phase & WHEEL_SCAN_PHASE_THIRD) != 0 &&
        (phase & (WHEEL_SCAN_PHASE_FIRST | WHEEL_SCAN_PHASE_SECOND)) == 0 &&
        output->third_glyph_marker) {
        encoded |= 0x08u;
    }
    return encoded;
}

/**
 * @brief Converts a raw seven-segment glyph to its protocol character.
 *
 * Recognizes decimal digits, both accepted one and seven patterns, parentheses, R, and N after
 * discarding the decimal-point bit. Unsupported patterns become a space.
 *
 * @param[in] glyph Raw seven-segment glyph.
 * @return Character used by the attached-wheel character display protocol.
 */
uint8_t wheel_display_output_character(uint8_t glyph) {
    switch (glyph & 0x7fU) {
    case 0x3f:
        return '0';
    case 0x06:
    case 0x30:
        return '1';
    case 0x5b:
        return '2';
    case 0x4f:
        return '3';
    case 0x66:
        return '4';
    case 0x6d:
        return '5';
    case 0x7d:
        return '6';
    case 0x07:
    case 0x27:
        return '7';
    case 0x7f:
        return '8';
    case 0x6f:
        return '9';
    case 0x39:
        return '(';
    case 0x0f:
        return ')';
    case 0x50:
        return 'R';
    case 0x54:
        return 'N';
    default:
        return ' ';
    }
}
