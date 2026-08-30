#include "display/bitmap.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Draws a packed four-bit grayscale bitmap.
 *
 * Reads the high nibble before the low nibble, advances the source to the next byte after each
 * pixel pair, clamps colors to the selected maximum, and optionally inverts the result.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] pixels Row-major packed grayscale pixels.
 * @param[in] x Left bitmap coordinate.
 * @param[in] y Top bitmap coordinate.
 * @param[in] width Bitmap width in pixels.
 * @param[in] height Bitmap height in pixels.
 * @param[in] inverted Whether to invert each clamped color.
 * @param[in] maximum_color Greatest source color accepted without clamping.
 */
void display_bitmap_draw(DisplayFramebuffer framebuffer, const uint8_t *pixels, uint16_t x,
                         uint16_t y, uint16_t width, uint16_t height, bool inverted,
                         uint8_t maximum_color) {
    uint16_t source = 0;
    for (uint16_t row = 0; row < height; row++) {
        for (uint16_t column = 0; column < width; column++) {
            uint8_t packed = pixels[source];
            uint8_t color = (column & 1u) == 0 ? packed >> 4 : packed & 0x0fu;
            if (color > maximum_color) {
                color = maximum_color;
            }
            if (inverted) {
                color = (uint8_t)~color;
            }
            display_framebuffer_set_pixel(framebuffer, (uint16_t)(x + column), (uint16_t)(y + row),
                                          color);
            if ((column & 1u) != 0) {
                source++;
            }
        }
        if ((width & 1u) != 0) {
            source++;
        }
    }
}
