#ifndef OPENTEC_BASE_DISPLAY_NOTICE_H
#define OPENTEC_BASE_DISPLAY_NOTICE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

void display_notice_render_torque_disabled(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE],
                                           bool visible);

#endif
