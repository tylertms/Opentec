#ifndef OPENTEC_BASE_DISPLAY_FRAMEBUFFER_H
#define OPENTEC_BASE_DISPLAY_FRAMEBUFFER_H

#include <stdint.h>

/**
 * @brief Defines the packed local-display framebuffer dimensions.
 *
 * Two four-bit grayscale pixels occupy each byte, so the 256-by-64 panel requires 8192 bytes.
 */
enum {
    DISPLAY_FRAMEBUFFER_WIDTH = 256, /**< Framebuffer width in pixels. */
    DISPLAY_FRAMEBUFFER_HEIGHT = 64, /**< Framebuffer height in pixels. */
    DISPLAY_FRAMEBUFFER_SIZE = 8192, /**< Framebuffer storage size in bytes. */
};

#if defined(__XC16__) && !defined(OPENTEC_SIMULATOR_TEST)
/**
 * @brief Identifies writable display framebuffer storage.
 *
 * The embedded build places the pixel buffer in extended data space for the display transfer.
 */
typedef __eds__ uint8_t *DisplayFramebuffer;
/**
 * @brief Identifies read-only display framebuffer storage.
 *
 * This alias is used when code consumes framebuffer data without modifying it.
 */
typedef const __eds__ uint8_t *ConstDisplayFramebuffer;
#else
/**
 * @brief Identifies writable display framebuffer storage.
 *
 * The simulator uses ordinary process-addressable byte storage for framebuffer pixels.
 */
typedef uint8_t *DisplayFramebuffer;
/**
 * @brief Identifies read-only display framebuffer storage.
 *
 * This alias is used when code consumes framebuffer data without modifying it.
 */
typedef const uint8_t *ConstDisplayFramebuffer;
#endif

/**
 * @brief Clears the display framebuffer.
 *
 * Sets every byte in the packed 256-by-64 grayscale buffer to zero.
 *
 * @param[out] framebuffer Framebuffer to clear.
 */
void display_framebuffer_clear(DisplayFramebuffer framebuffer);

/**
 * @brief Writes one grayscale pixel into the framebuffer.
 *
 * Packs even columns into a byte's high nibble and odd columns into its low nibble; coordinates
 * outside the controller's legal 0..254 by 0..62 drawable range are ignored. The packed storage
 * still reserves the complete 256-by-64 transfer window.
 *
 * @param[in,out] framebuffer Framebuffer whose pixel data is updated.
 * @param[in] x Pixel column.
 * @param[in] y Pixel row.
 * @param[in] value Four-bit grayscale value; higher bits are discarded.
 */
void display_framebuffer_set_pixel(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y,
                                   uint8_t value);

#endif
