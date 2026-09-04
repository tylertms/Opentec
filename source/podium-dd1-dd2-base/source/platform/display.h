#ifndef OPENTEC_BASE_PLATFORM_DISPLAY_H
#define OPENTEC_BASE_PLATFORM_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Initializes the display bus and controller.
 *
 * Configures the parallel display bus, applies controller reset timing, and sends controller setup
 * commands.
 */
void platform_display_init(void);

/**
 * @brief Resets the display controller.
 *
 * Applies the required high-low-high reset pulse timing.
 */
void platform_display_reset(void);

/**
 * @brief Drives the display controller reset input.
 *
 * Sets the reset input high when the controller must remain released and low when the controller
 * must be held in reset. This nonblocking primitive is used by runtime bridge reset scheduling.
 *
 * @param[in] high True to release reset; false to assert reset.
 */
void platform_display_reset_set(bool high);

/**
 * @brief Writes one complete display framebuffer.
 *
 * Selects the display frame window and starts a DMA transfer from framebuffer storage.
 *
 * @param[in] framebuffer Packed display framebuffer to transmit.
 */
void platform_display_write_frame(ConstDisplayFramebuffer framebuffer);

#endif
