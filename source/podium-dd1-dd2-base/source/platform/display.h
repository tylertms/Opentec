#ifndef OPENTEC_BASE_PLATFORM_DISPLAY_H
#define OPENTEC_BASE_PLATFORM_DISPLAY_H

#include <stdint.h>

#include "display/framebuffer.h"

void platform_display_init(void);
void platform_display_reset(void);
void platform_display_write_frame(ConstDisplayFramebuffer framebuffer);

#endif
