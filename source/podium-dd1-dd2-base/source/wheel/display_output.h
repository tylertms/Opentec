#ifndef OPENTEC_BASE_WHEEL_DISPLAY_OUTPUT_H
#define OPENTEC_BASE_WHEEL_DISPLAY_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_DISPLAY_GLYPH_COUNT = 3,
    WHEEL_SCAN_PHASE_FIRST = 1,
    WHEEL_SCAN_PHASE_SECOND = 2,
    WHEEL_SCAN_PHASE_THIRD = 4,
    WHEEL_SCAN_PHASE_AUXILIARY = 8,
};

typedef struct {
    uint8_t glyphs[WHEEL_DISPLAY_GLYPH_COUNT];
    uint8_t auxiliary;
    bool third_glyph_marker;
} WheelDisplayOutput;

uint8_t wheel_display_output_encode(const WheelDisplayOutput *output, uint8_t phase);
uint8_t wheel_display_output_character(uint8_t glyph);

#endif
