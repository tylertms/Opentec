#ifndef OPENTEC_BASE_DISPLAY_TEXT_H
#define OPENTEC_BASE_DISPLAY_TEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "display/font.h"
#include "display/framebuffer.h"

/**
 * @brief Selects one of the two official display fonts for compatibility wrappers.
 */
enum {
    DISPLAY_TEXT_FONT_10 = 1,
    DISPLAY_TEXT_FONT_21 = 2,
};

/**
 * @brief Measures text using variable-width official glyphs.
 *
 * Unsupported characters consume no width, and the leading renderer column is not included.
 *
 * @param[in] font Official font table.
 * @param[in] text Null-terminated text.
 * @return Sum of drawable glyph widths.
 */
uint16_t display_text_width_for_font(const DisplayFont *font, const char *text);

/**
 * @brief Draws text with official glyph packing and polarity.
 *
 * The supplied x coordinate is the leading background column. Every glyph pixel, including
 * background pixels, is written and glyph origins advance by their variable widths.
 *
 * @param[in,out] framebuffer Packed display framebuffer.
 * @param[in] font Official font table.
 * @param[in] text Null-terminated text.
 * @param[in] x Leading background column.
 * @param[in] y Top row.
 * @param[in] invert Invert foreground and background pixels.
 */
void display_text_draw_with_font(DisplayFramebuffer framebuffer, const DisplayFont *font,
                                 const char *text, uint16_t x, uint16_t y, bool invert);

/**
 * @brief Draws official-font text centered using its variable glyph width.
 *
 * The centering calculation excludes the renderer's leading background column.
 */
void display_text_draw_centered_with_font(DisplayFramebuffer framebuffer, const DisplayFont *font,
                                          const char *text, uint16_t y, bool invert);

/**
 * @brief Measures text through the compatibility font selector.
 *
 * Scale 1 selects Font10 and scale 2 selects Font21. Other scale values select no font.
 *
 * @param[in] text Null-terminated text.
 * @param[in] scale Compatibility font selector.
 * @return Sum of drawable glyph widths.
 */
uint16_t display_text_width(const char *text, uint8_t scale);

/**
 * @brief Draws text through the compatibility font selector.
 *
 * Scale 1 selects Font10 and scale 2 selects Font21. The supplied x coordinate is the leading
 * background column, and every glyph pixel is written with the selected foreground value.
 *
 * @param[in,out] framebuffer Packed display framebuffer.
 * @param[in] text Null-terminated text.
 * @param[in] x Leading background column.
 * @param[in] y Top row.
 * @param[in] scale Compatibility font selector.
 * @param[in] color Four-bit foreground grayscale value.
 */
void display_text_draw(DisplayFramebuffer framebuffer, const char *text, uint16_t x, uint16_t y,
                       uint8_t scale, uint8_t color);

/**
 * @brief Draws compatibility-selector text centered using variable glyph widths.
 *
 * @param[in,out] framebuffer Packed display framebuffer.
 * @param[in] text Null-terminated text.
 * @param[in] y Top row.
 * @param[in] scale Compatibility font selector.
 * @param[in] color Four-bit foreground grayscale value.
 */
void display_text_draw_centered(DisplayFramebuffer framebuffer, const char *text, uint16_t y,
                                uint8_t scale, uint8_t color);

#endif
