#ifndef OPENTEC_BASE_DISPLAY_IDENTITY_LOGO_H
#define OPENTEC_BASE_DISPLAY_IDENTITY_LOGO_H

#include <stdint.h>

#include "display/framebuffer.h"

void display_identity_logo_draw(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y);

#endif
