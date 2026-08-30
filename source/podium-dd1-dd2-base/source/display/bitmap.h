#ifndef OPENTEC_BASE_DISPLAY_BITMAP_H
#define OPENTEC_BASE_DISPLAY_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

void display_bitmap_draw(DisplayFramebuffer framebuffer, const uint8_t *pixels, uint16_t x,
                         uint16_t y, uint16_t width, uint16_t height, bool inverted,
                         uint8_t maximum_color);

#endif
