#ifndef OPENTEC_BASE_DISPLAY_FRAMEBUFFER_H
#define OPENTEC_BASE_DISPLAY_FRAMEBUFFER_H

#include <stdint.h>

enum {
    DISPLAY_FRAMEBUFFER_WIDTH = 256,
    DISPLAY_FRAMEBUFFER_HEIGHT = 64,
    DISPLAY_FRAMEBUFFER_SIZE = 8192,
};

void display_framebuffer_clear(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE]);
void display_framebuffer_set_pixel(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x,
                                   uint16_t y, uint8_t value);

#endif
