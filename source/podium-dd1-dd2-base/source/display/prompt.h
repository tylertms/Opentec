#ifndef OPENTEC_BASE_DISPLAY_PROMPT_H
#define OPENTEC_BASE_DISPLAY_PROMPT_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

typedef struct {
    bool input_seen;
} DisplayPrompt;

void display_prompt_render(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], bool visible);
void display_prompt_render_bite_point(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], bool visible,
                                      uint8_t percent);
bool display_prompt_update(DisplayPrompt *prompt, bool visible, bool input_active);

#endif
