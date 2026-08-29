#ifndef OPENTEC_BASE_DISPLAY_TEXT_H
#define OPENTEC_BASE_DISPLAY_TEXT_H

#include <stdint.h>

#include "display/framebuffer.h"

uint16_t display_text_width(const char *text, uint8_t scale);
void display_text_draw(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], const char *text, uint16_t x,
                       uint16_t y, uint8_t scale, uint8_t color);
void display_text_draw_centered(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], const char *text,
                                uint16_t y, uint8_t scale, uint8_t color);

#endif
