#include "display/prompt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display/framebuffer.h"

enum {
    GLYPH_WIDTH = 5,
    GLYPH_HEIGHT = 7,
    GLYPH_ADVANCE = 6,
    GLYPH_SCALE = 2,
    PROMPT_COLOR = 15,
};

typedef struct {
    char character;
    uint8_t rows[GLYPH_HEIGHT];
} Glyph;

static const Glyph glyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'?', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
    {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'I', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'N', {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11}},
    {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'Q', {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
};

static const Glyph *find_glyph(char character) {
    for (size_t index = 0; index < sizeof(glyphs) / sizeof(glyphs[0]); index++) {
        if (glyphs[index].character == character) {
            return &glyphs[index];
        }
    }
    return &glyphs[0];
}

static uint16_t text_width(const char *text) {
    uint16_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return (uint16_t)((length * GLYPH_ADVANCE - 1) * GLYPH_SCALE);
}

static void draw_text(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], const char *text, uint16_t y) {
    uint16_t x = (DISPLAY_FRAMEBUFFER_WIDTH - text_width(text)) / 2;
    for (uint16_t index = 0; text[index] != '\0'; index++) {
        const Glyph *glyph = find_glyph(text[index]);
        for (uint16_t row = 0; row < GLYPH_HEIGHT; row++) {
            for (uint16_t column = 0; column < GLYPH_WIDTH; column++) {
                if ((glyph->rows[row] & (uint8_t)(1u << (GLYPH_WIDTH - column - 1))) == 0) {
                    continue;
                }
                for (uint16_t scale_y = 0; scale_y < GLYPH_SCALE; scale_y++) {
                    for (uint16_t scale_x = 0; scale_x < GLYPH_SCALE; scale_x++) {
                        display_framebuffer_set_pixel(
                            framebuffer,
                            (uint16_t)(x + index * GLYPH_ADVANCE * GLYPH_SCALE +
                                       column * GLYPH_SCALE + scale_x),
                            (uint16_t)(y + row * GLYPH_SCALE + scale_y), PROMPT_COLOR);
                    }
                }
            }
        }
    }
}

void display_prompt_render(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], bool visible) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }
    draw_text(framebuffer, "ATTENTION", 10);
    draw_text(framebuffer, "ENABLE TORQUE?", 38);
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
