#include "display/high_torque_icon.h"

#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Defines the high-torque icon bitmap dimensions.
 *
 * The packed bitmap stores two four-bit grayscale pixels per byte for each icon row.
 */
enum {
    HIGH_TORQUE_ICON_WIDTH = 11,    /**< Icon width in pixels. */
    HIGH_TORQUE_ICON_HEIGHT = 10,   /**< Icon height in pixels. */
    HIGH_TORQUE_ICON_ROW_BYTES = 6, /**< Packed bytes in one icon row. */
};

/**
 * @brief Stores the packed high-torque icon bitmap.
 *
 * Pixels are arranged in row-major order with the high nibble preceding the low nibble.
 */
static const uint8_t high_torque_icon[] = {
    0x00, 0x00, 0x3f, 0x30, 0x00, 0x00, 0x00, 0x01, 0xef, 0xe1, 0x00, 0x00, 0x00, 0x0c, 0xff,
    0xfc, 0x00, 0x00, 0x00, 0x3f, 0xf0, 0xff, 0x30, 0x00, 0x00, 0xcf, 0xf0, 0xff, 0xc0, 0x00,
    0x03, 0xff, 0xf0, 0xff, 0xf3, 0x00, 0x0c, 0xff, 0xf0, 0xff, 0xfc, 0x00, 0x5f, 0xff, 0xff,
    0xff, 0xff, 0x50, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
};

/**
 * @brief Draws the high-torque operator icon.
 *
 * Expands the packed eleven-by-ten grayscale icon at the requested display position.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Left icon coordinate.
 * @param[in] y Top icon coordinate.
 */
void display_high_torque_icon_draw(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y) {
    for (uint16_t row = 0; row < HIGH_TORQUE_ICON_HEIGHT; row++) {
        for (uint16_t column = 0; column < HIGH_TORQUE_ICON_WIDTH; column++) {
            uint8_t packed = high_torque_icon[row * HIGH_TORQUE_ICON_ROW_BYTES + column / 2];
            uint8_t color = (column & 1u) == 0 ? packed >> 4 : packed & 0x0fu;
            display_framebuffer_set_pixel(framebuffer, (uint16_t)(x + column), (uint16_t)(y + row),
                                          color);
        }
    }
}
