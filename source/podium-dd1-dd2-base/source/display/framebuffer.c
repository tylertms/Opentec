#include "display/framebuffer.h"

#include <stdint.h>

enum {
    DISPLAY_DRAWABLE_MAX_X = 254,
    DISPLAY_DRAWABLE_MAX_Y = 62,
    DISPLAY_ROW_SIZE = DISPLAY_FRAMEBUFFER_WIDTH / 2,
};

/**
 * @brief Clears the complete display framebuffer.
 *
 * Writes zero to every byte in the 8192-byte packed grayscale framebuffer.
 *
 * @param[out] framebuffer Packed 256-by-64 grayscale framebuffer.
 */
void display_framebuffer_clear(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE]) {
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        framebuffer[index] = 0;
    }
}

/**
 * @brief Writes one four-bit grayscale display pixel.
 *
 * Packs even columns into the high nibble and odd columns into the low nibble of each framebuffer
 * byte. Coordinates beyond column 254 or row 62 are ignored.
 *
 * @param[in,out] framebuffer Packed 256-by-64 grayscale framebuffer.
 * @param[in] x Pixel column.
 * @param[in] y Pixel row.
 * @param[in] value Four-bit grayscale value; higher bits are discarded.
 */
void display_framebuffer_set_pixel(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x,
                                   uint16_t y, uint8_t value) {
    if (x > DISPLAY_DRAWABLE_MAX_X || y > DISPLAY_DRAWABLE_MAX_Y) {
        return;
    }

    uint16_t offset = y * DISPLAY_ROW_SIZE + x / 2;
    if ((x & 1u) != 0) {
        framebuffer[offset] = (framebuffer[offset] & 0xf0u) | (value & 0x0fu);
    } else {
        framebuffer[offset] = (framebuffer[offset] & 0x0fu) | ((value & 0x0fu) << 4);
    }
}
