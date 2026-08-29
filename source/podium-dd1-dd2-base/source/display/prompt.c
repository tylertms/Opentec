#include "display/prompt.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"

enum {
    GLYPH_HEIGHT = 7,
    GLYPH_SCALE = 2,
    PROMPT_COLOR = 15,
};

/**
 * @brief Renders the force-output acknowledgement prompt.
 *
 * Clears the display and, while visible, draws both centered prompt lines at their selected rows.
 *
 * @param[out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while the acknowledgement prompt owns the display.
 */
void display_prompt_render(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], bool visible) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }
    display_text_draw_centered(framebuffer, "ATTENTION", 10, GLYPH_SCALE, PROMPT_COLOR);
    display_text_draw_centered(framebuffer, "ENABLE TORQUE?", 38, GLYPH_SCALE, PROMPT_COLOR);
}

/**
 * @brief Renders the active paddle bite-point value.
 *
 * Clears the framebuffer and, while visible, renders the unpadded decimal percentage
 * right-aligned in a centered three-character field.
 *
 * @param[out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while paddle bite-point adjustment owns the display.
 * @param[in] percent Current bite-point percentage from zero through one hundred.
 */
void display_prompt_render_bite_point(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], bool visible,
                                      uint8_t percent) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }

    char text[4] = {' ', ' ', '0', '\0'};
    if (percent >= 100) {
        text[0] = '1';
        text[1] = '0';
        text[2] = '0';
    } else if (percent >= 10) {
        text[1] = (char)('0' + percent / 10);
        text[2] = (char)('0' + percent % 10);
    } else {
        text[2] = (char)('0' + percent);
    }
    display_text_draw_centered(framebuffer, text,
                               (DISPLAY_FRAMEBUFFER_HEIGHT - GLYPH_HEIGHT * GLYPH_SCALE) / 2,
                               GLYPH_SCALE, PROMPT_COLOR);
}

/**
 * @brief Detects acknowledgement input for an active display prompt.
 *
 * Latches any active input and reports acknowledgement only after all input is released. Hiding the
 * prompt clears a partially completed interaction.
 *
 * @param[in,out] prompt Prompt input latch.
 * @param[in] visible True while the acknowledgement prompt is displayed.
 * @param[in] input_active True while any caller-selected acknowledgement input is active.
 * @return True once after a latched input is released while the prompt remains visible.
 */
bool display_prompt_update(DisplayPrompt *prompt, bool visible, bool input_active) {
    if (!visible) {
        prompt->input_seen = false;
        return false;
    }
    if (input_active) {
        prompt->input_seen = true;
        return false;
    }
    if (!prompt->input_seen) {
        return false;
    }
    prompt->input_seen = false;
    return true;
}
