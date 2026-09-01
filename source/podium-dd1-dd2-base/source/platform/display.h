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
 * @brief Writes one complete display framebuffer.
 *
 * Selects the display frame window and starts a DMA transfer from framebuffer storage.
 *
 * @param[in] framebuffer Packed display framebuffer to transmit.
 */
void platform_display_write_frame(ConstDisplayFramebuffer framebuffer);

/**
 * @brief Reports completion of the current framebuffer transfer.
 *
 * Requires both the DMA block and the final parallel-master byte transfer to finish.
 *
 * @return True when no framebuffer byte remains in flight; otherwise false.
 */
bool platform_display_frame_complete(void);

#endif
