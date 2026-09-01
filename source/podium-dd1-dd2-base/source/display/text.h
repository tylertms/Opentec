#ifndef OPENTEC_BASE_DISPLAY_TEXT_H
#define OPENTEC_BASE_DISPLAY_TEXT_H

#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Measures text rendered with the built-in display font.
 *
 * Returns the width of the glyphs and inter-glyph advances at the requested integer scale without
 * including trailing spacing after the final glyph.
 *
 * @param[in] text Null-terminated text to measure.
 * @param[in] scale Integer pixel scale.
 * @return Rendered text width in pixels, or zero for empty text or a zero scale.
 */
uint16_t display_text_width(const char *text, uint8_t scale);

/**
 * @brief Draws text into the grayscale framebuffer.
 *
 * Expands each supported glyph at the requested integer scale and writes lit pixels with the
 * selected grayscale color.
 *
 * @param[in,out] framebuffer Framebuffer receiving the text pixels.
 * @param[in] text Null-terminated text to draw.
 * @param[in] x Left text coordinate.
 * @param[in] y Top text coordinate.
 * @param[in] scale Integer pixel scale.
 * @param[in] color Four-bit grayscale value.
 */
void display_text_draw(DisplayFramebuffer framebuffer, const char *text, uint16_t x, uint16_t y,
                       uint8_t scale, uint8_t color);

/**
 * @brief Draws text centered horizontally in the framebuffer.
 *
 * Measures the text at the requested scale, clamps oversized text to the left edge, and renders
 * it at the supplied top row.
 *
 * @param[in,out] framebuffer Framebuffer receiving the text pixels.
 * @param[in] text Null-terminated text to draw.
 * @param[in] y Top text coordinate.
 * @param[in] scale Integer pixel scale.
 * @param[in] color Four-bit grayscale value.
 */
void display_text_draw_centered(DisplayFramebuffer framebuffer, const char *text, uint16_t y,
                                uint8_t scale, uint8_t color);

#endif
