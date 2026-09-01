#ifndef OPENTEC_BASE_PLATFORM_DISPLAY_H
#define OPENTEC_BASE_PLATFORM_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

void platform_display_init(void);
void platform_display_reset(void);
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
