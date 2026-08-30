#ifndef OPENTEC_BASE_DISPLAY_FRAMEBUFFER_H
#define OPENTEC_BASE_DISPLAY_FRAMEBUFFER_H

#include <stdint.h>

enum {
    DISPLAY_FRAMEBUFFER_WIDTH = 256,
    DISPLAY_FRAMEBUFFER_HEIGHT = 64,
    DISPLAY_FRAMEBUFFER_SIZE = 8192,
};

#if defined(__XC16__) && !defined(OPENTEC_SIM30_TEST)
typedef __eds__ uint8_t *DisplayFramebuffer;
typedef const __eds__ uint8_t *ConstDisplayFramebuffer;
#else
typedef uint8_t *DisplayFramebuffer;
typedef const uint8_t *ConstDisplayFramebuffer;
#endif

void display_framebuffer_clear(DisplayFramebuffer framebuffer);
void display_framebuffer_set_pixel(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y,
                                   uint8_t value);

#endif
