#include "wheel/display_output.h"

#include <stdint.h>

/**
 * Encodes the active display output for an attached-wheel scan phase.
 *
 * @param output Three display glyphs, the auxiliary byte, and the phase-4 marker state.
 * @param phase Scan phase bit field. Lower phases take priority when multiple bits are set.
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
    if ((phase & WHEEL_SCAN_PHASE_THIRD) != 0 && output->third_glyph_marker) {
        encoded |= 0x08u;
    }
    return encoded;
}
