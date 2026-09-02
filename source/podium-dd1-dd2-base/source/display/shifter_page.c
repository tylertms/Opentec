#include "display/shifter_page.h"

#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"

enum {
    SHIFTER_PAGE_COLOR = 15,
    SHIFTER_PAGE_GLYPH_Y = 18,
    SHIFTER_PAGE_GLYPH_X = 120,
    SHIFTER_PAGE_WAITING_FIRST_X = 95,
    SHIFTER_PAGE_WAITING_SECOND_X = 120,
    SHIFTER_PAGE_DIAGNOSTIC_TITLE_X = 2,
    SHIFTER_PAGE_DIAGNOSTIC_TITLE_Y = 0x11,
    SHIFTER_PAGE_DIAGNOSTIC_TEXT_X = 3,
    SHIFTER_PAGE_DIAGNOSTIC_LINE_ONE_Y = 0x24,
    SHIFTER_PAGE_DIAGNOSTIC_LINE_TWO_Y = 0x2e,
    SHIFTER_PAGE_DIAGNOSTIC_FRAME_X = 1,
    SHIFTER_PAGE_DIAGNOSTIC_FRAME_TOP = 0x10,
    SHIFTER_PAGE_DIAGNOSTIC_FRAME_BOTTOM = 0x38,
};

enum {
    SHIFTER_PAGE_SEGMENT_TOP = 1 << 0,
    SHIFTER_PAGE_SEGMENT_UPPER_RIGHT = 1 << 1,
    SHIFTER_PAGE_SEGMENT_LOWER_RIGHT = 1 << 2,
    SHIFTER_PAGE_SEGMENT_BOTTOM = 1 << 3,
    SHIFTER_PAGE_SEGMENT_LOWER_LEFT = 1 << 4,
    SHIFTER_PAGE_SEGMENT_UPPER_LEFT = 1 << 5,
    SHIFTER_PAGE_SEGMENT_MIDDLE = 1 << 6,
    SHIFTER_PAGE_DECIMAL_POINT = 1 << 7,
};

static void draw_rectangle(DisplayFramebuffer framebuffer, uint16_t left, uint16_t top,
                           uint16_t right, uint16_t bottom, uint8_t color) {
    for (uint16_t y = top; y <= bottom; y++) {
        for (uint16_t x = left; x <= right; x++) {
            display_framebuffer_set_pixel(framebuffer, x, y, color);
        }
    }
}

static void draw_glyph(DisplayFramebuffer framebuffer, uint16_t x, uint8_t glyph) {
    if ((glyph & SHIFTER_PAGE_SEGMENT_TOP) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 3), SHIFTER_PAGE_GLYPH_Y, (uint16_t)(x + 18),
                       SHIFTER_PAGE_GLYPH_Y + 2, SHIFTER_PAGE_COLOR);
    }
    if ((glyph & SHIFTER_PAGE_SEGMENT_UPPER_RIGHT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 16), SHIFTER_PAGE_GLYPH_Y + 3,
                       (uint16_t)(x + 19), SHIFTER_PAGE_GLYPH_Y + 16, SHIFTER_PAGE_COLOR);
    }
    if ((glyph & SHIFTER_PAGE_SEGMENT_LOWER_RIGHT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 16), SHIFTER_PAGE_GLYPH_Y + 20,
                       (uint16_t)(x + 19), SHIFTER_PAGE_GLYPH_Y + 33, SHIFTER_PAGE_COLOR);
    }
    if ((glyph & SHIFTER_PAGE_SEGMENT_BOTTOM) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 3), SHIFTER_PAGE_GLYPH_Y + 34,
                       (uint16_t)(x + 18), SHIFTER_PAGE_GLYPH_Y + 36, SHIFTER_PAGE_COLOR);
    }
    if ((glyph & SHIFTER_PAGE_SEGMENT_LOWER_LEFT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 1), SHIFTER_PAGE_GLYPH_Y + 20, (uint16_t)(x + 4),
                       SHIFTER_PAGE_GLYPH_Y + 33, SHIFTER_PAGE_COLOR);
    }
    if ((glyph & SHIFTER_PAGE_SEGMENT_UPPER_LEFT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 1), SHIFTER_PAGE_GLYPH_Y + 3, (uint16_t)(x + 4),
                       SHIFTER_PAGE_GLYPH_Y + 16, SHIFTER_PAGE_COLOR);
    }
    if ((glyph & SHIFTER_PAGE_SEGMENT_MIDDLE) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 3), SHIFTER_PAGE_GLYPH_Y + 17,
                       (uint16_t)(x + 18), SHIFTER_PAGE_GLYPH_Y + 19, SHIFTER_PAGE_COLOR);
    }
    if ((glyph & SHIFTER_PAGE_DECIMAL_POINT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 21), SHIFTER_PAGE_GLYPH_Y + 33,
                       (uint16_t)(x + 24), SHIFTER_PAGE_GLYPH_Y + 36, SHIFTER_PAGE_COLOR);
    }
}

static void draw_diagnostic_frame(DisplayFramebuffer framebuffer) {
    for (uint16_t y = SHIFTER_PAGE_DIAGNOSTIC_FRAME_TOP; y <= SHIFTER_PAGE_DIAGNOSTIC_FRAME_BOTTOM;
         y++) {
        display_framebuffer_set_pixel(framebuffer, SHIFTER_PAGE_DIAGNOSTIC_FRAME_X, y, 8);
    }
    for (uint16_t x = SHIFTER_PAGE_DIAGNOSTIC_FRAME_X; x < DISPLAY_FRAMEBUFFER_WIDTH; x++) {
        display_framebuffer_set_pixel(framebuffer, x, SHIFTER_PAGE_DIAGNOSTIC_FRAME_BOTTOM, 8);
    }
}

static void render_waiting(DisplayFramebuffer framebuffer) {
    draw_glyph(framebuffer, SHIFTER_PAGE_WAITING_FIRST_X, 0x06);
    draw_glyph(framebuffer, SHIFTER_PAGE_WAITING_SECOND_X, 0x7f);
}

static void render_label(DisplayFramebuffer framebuffer, const char *text) {
    display_text_draw_centered_with_font(framebuffer, &display_font_10_00c988, text, 25, false);
}

static void render_diagnostic(DisplayFramebuffer framebuffer) {
    display_text_draw_with_font(framebuffer, &display_font_21_00aba6, "Shifter Calibration Mode",
                                SHIFTER_PAGE_DIAGNOSTIC_TITLE_X, SHIFTER_PAGE_DIAGNOSTIC_TITLE_Y,
                                false);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988,
                                "Set the gear as shown on the steering wheel display and",
                                SHIFTER_PAGE_DIAGNOSTIC_TEXT_X, SHIFTER_PAGE_DIAGNOSTIC_LINE_ONE_Y,
                                false);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988,
                                "press the acknowledge button to teach the gear position",
                                SHIFTER_PAGE_DIAGNOSTIC_TEXT_X, SHIFTER_PAGE_DIAGNOSTIC_LINE_TWO_Y,
                                false);
    draw_diagnostic_frame(framebuffer);
}

/**
 * @brief Renders the local OLED shifter presentation.
 *
 * Selects the compact glyph, entry labels, or calibration diagnostic records owned by the local
 * page state.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] presentation Current local shifter presentation.
 */
void display_shifter_page_render(DisplayFramebuffer framebuffer,
                                 const ShifterLocalDisplay *presentation) {
    display_framebuffer_clear(framebuffer);
    if (presentation == NULL || presentation->kind == SHIFTER_LOCAL_DISPLAY_NONE) {
        return;
    }
    if (presentation->kind == SHIFTER_LOCAL_DISPLAY_GEAR) {
        draw_glyph(framebuffer, SHIFTER_PAGE_GLYPH_X, presentation->glyph);
        return;
    }
    switch (presentation->calibration_prompt) {
    case H_PATTERN_CALIBRATION_PROMPT_WAITING:
        render_waiting(framebuffer);
        break;
    case H_PATTERN_CALIBRATION_PROMPT_SHIFTER:
        render_label(framebuffer, "SFT");
        break;
    case H_PATTERN_CALIBRATION_PROMPT_CALIBRATION:
        render_label(framebuffer, "CAL");
        break;
    case H_PATTERN_CALIBRATION_PROMPT_POSITION:
        render_diagnostic(framebuffer);
        break;
    case H_PATTERN_CALIBRATION_PROMPT_NONE:
        break;
    }
}
