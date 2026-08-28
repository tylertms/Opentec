#ifndef OPENTEC_BASE_PLATFORM_DISPLAY_H
#define OPENTEC_BASE_PLATFORM_DISPLAY_H

#include <stdint.h>

enum {
    PLATFORM_DISPLAY_FRAMEBUFFER_SIZE = 8192,
};

void platform_display_init(void);
void platform_display_write_frame(const uint8_t framebuffer[PLATFORM_DISPLAY_FRAMEBUFFER_SIZE]);

#endif
