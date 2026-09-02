#include "display/high_torque_icon.h"

#include <stdint.h>

#include "display/bitmap.h"
#include "display/framebuffer.h"

/**
 * @brief Defines the high-torque icon bitmap dimensions.
 *
 * The packed bitmap stores two four-bit grayscale pixels per byte for each icon row.
 */
enum {
    HIGH_TORQUE_ICON_WIDTH = 11,  /**< Icon width in pixels. */
    HIGH_TORQUE_ICON_HEIGHT = 10, /**< Icon height in pixels. */
};

/**
 * @brief Stores the packed high-torque icon bitmap.
 *
 * Pixels are arranged in row-major order with the high nibble preceding the low nibble, matching
 * the official bitmap at binary address 0xa930.
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
 * @param[in] inverted Whether to invert each icon pixel.
 */
void display_high_torque_icon_draw(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y,
                                   bool inverted) {
    display_bitmap_draw(framebuffer, high_torque_icon, x, y, HIGH_TORQUE_ICON_WIDTH,
                        HIGH_TORQUE_ICON_HEIGHT, inverted, 0x0f);
}
