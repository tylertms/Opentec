#include "display/shifter_page.h"

#include <stdint.h>

#include "display/framebuffer.h"
#include "display/icon_bitmaps.h"
#include "display/text.h"

enum {
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
    SHIFTER_PAGE_DIAGNOSTIC_FRAME_RIGHT = 0xff,
};

static void draw_glyph(DisplayBitmapQueue *queue, uint16_t x, uint8_t glyph) {
    display_icon_group_draw_queued(queue, x, SHIFTER_PAGE_GLYPH_Y, glyph);
}

static void draw_diagnostic_frame(DisplayFramebuffer framebuffer) {
    for (uint16_t y = SHIFTER_PAGE_DIAGNOSTIC_FRAME_TOP;
         y < SHIFTER_PAGE_DIAGNOSTIC_FRAME_BOTTOM; y++) {
        display_framebuffer_set_pixel(framebuffer, SHIFTER_PAGE_DIAGNOSTIC_FRAME_X, y, 8);
    }
    for (uint16_t x = SHIFTER_PAGE_DIAGNOSTIC_FRAME_X; x < SHIFTER_PAGE_DIAGNOSTIC_FRAME_RIGHT;
         x++) {
        display_framebuffer_set_pixel(framebuffer, x, SHIFTER_PAGE_DIAGNOSTIC_FRAME_BOTTOM, 8);
    }
}

static void render_waiting(DisplayBitmapQueue *queue) {
    draw_glyph(queue, SHIFTER_PAGE_WAITING_FIRST_X, 0x06);
    draw_glyph(queue, SHIFTER_PAGE_WAITING_SECOND_X, 0x7f);
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
    DisplayBitmapQueue queue;
    display_bitmap_queue_reset(&queue);
    display_shifter_page_render_queued(framebuffer, &queue, presentation);
    display_bitmap_queue_finish(&queue, framebuffer);
}

void display_shifter_page_render_queued(DisplayFramebuffer framebuffer, DisplayBitmapQueue *queue,
                                        const ShifterLocalDisplay *presentation) {
    display_framebuffer_clear(framebuffer);
    if (presentation == NULL || presentation->kind == SHIFTER_LOCAL_DISPLAY_NONE) {
        return;
    }
    if (presentation->kind == SHIFTER_LOCAL_DISPLAY_GEAR) {
        draw_glyph(queue, SHIFTER_PAGE_GLYPH_X, presentation->glyph);
        return;
    }
    switch (presentation->calibration_prompt) {
    case H_PATTERN_CALIBRATION_PROMPT_WAITING:
        render_waiting(queue);
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
