#ifndef OPENTEC_BASE_DISPLAY_BITMAP_H
#define OPENTEC_BASE_DISPLAY_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Draws a packed four-bit grayscale bitmap.
 *
 * Reads row-major pixels from high nibble to low nibble, clamps each pixel to the selected maximum
 * color, optionally inverts it, and writes the result at the requested framebuffer position.
 *
 * @param[in,out] framebuffer Framebuffer receiving the bitmap pixels.
 * @param[in] pixels Row-major pixels packed two four-bit values per byte.
 * @param[in] x Left bitmap coordinate.
 * @param[in] y Top bitmap coordinate.
 * @param[in] width Bitmap width in pixels.
 * @param[in] height Bitmap height in pixels.
 * @param[in] inverted Whether to invert each clamped pixel value.
 * @param[in] maximum_color Greatest source color accepted before clamping.
 */
void display_bitmap_draw(DisplayFramebuffer framebuffer, const uint8_t *pixels, uint16_t x,
                         uint16_t y, uint16_t width, uint16_t height, bool inverted,
                         uint8_t maximum_color);

#endif
