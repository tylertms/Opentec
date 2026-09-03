#ifndef OPENTEC_BASE_WHEEL_DISPLAY_OUTPUT_H
#define OPENTEC_BASE_WHEEL_DISPLAY_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Display scan dimensions and phase flags. */
enum {
    WHEEL_DISPLAY_GLYPH_COUNT = 3,  /**< Number of primary display glyphs. */
    WHEEL_SCAN_PHASE_FIRST = 1,     /**< Selects the first glyph during scanning. */
    WHEEL_SCAN_PHASE_SECOND = 2,    /**< Selects the second glyph during scanning. */
    WHEEL_SCAN_PHASE_THIRD = 4,     /**< Selects the third glyph during scanning. */
    WHEEL_SCAN_PHASE_AUXILIARY = 8, /**< Selects the auxiliary display byte. */
};

/** @brief Three glyphs and the auxiliary byte shown by an attached-wheel display. */
typedef struct {
    uint8_t glyphs[WHEEL_DISPLAY_GLYPH_COUNT]; /**< Raw seven-segment glyphs. */
    uint8_t auxiliary;                         /**< Auxiliary display byte. */
    bool third_glyph_marker; /**< Whether the third glyph carries a marker during a third-only scan. */
} WheelDisplayOutput;

/**
 * @brief Encodes one attached-wheel display scan phase.
 *
 * Selects the first matching glyph phase, or returns the auxiliary byte unchanged when no glyph
 * phase is selected. Glyph segments are permuted into scan-bit order and lower phase bits take
 * priority.
 *
 * @param[in] output Display glyphs and auxiliary value to encode.
 * @param[in] phase Scan phase flags to select.
 * @return Encoded glyph or auxiliary byte, or zero when no phase is selected.
 */
uint8_t wheel_display_output_encode(const WheelDisplayOutput *output, uint8_t phase);

/**
 * @brief Converts one raw seven-segment glyph to a display character.
 *
 * Recognizes supported decimal digits, parentheses, R, and N after removing the decimal-point
 * bit. Unsupported patterns are represented by a space.
 *
 * @param[in] glyph Raw seven-segment glyph.
 * @return Character represented by the glyph, or a space for an unsupported pattern.
 */
uint8_t wheel_display_output_character(uint8_t glyph);

#endif
