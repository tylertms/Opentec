#include "display/auxiliary_calibration_page.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/icon_bitmaps.h"
#include "display/text.h"

/**
 * @brief Defines legacy-display colors and seven-segment bit assignments.
 *
 * The bit assignments match the raw glyph values supplied by the wheel-service display output.
 */
enum {
    AUXILIARY_CALIBRATION_COLOR = 15, /**< Foreground grayscale value for legacy display content. */
    AUXILIARY_CALIBRATION_GLYPH_Y = 18, /**< Glyph top coordinate. */
};

/**
 * @brief Draws one raw seven-segment wheel glyph.
 *
 * Maps segment bits zero through six to the standard bars and bit seven to the decimal point.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Left coordinate of the glyph.
 * @param[in] glyph Raw seven-segment glyph and decimal-point bit.
 */
static void draw_glyph(DisplayBitmapQueue *queue, uint16_t x, uint8_t glyph) {
    display_icon_group_draw_queued(queue, x, AUXILIARY_CALIBRATION_GLYPH_Y, glyph);
}

/**
 * @brief Updates the legacy display mirror.
 *
 * Retains the three selected wheel glyphs and whether the ITM session indicator is visible.
 *
 * @param[in,out] page Current legacy display mirror.
 * @param[in] glyphs Three raw seven-segment glyphs.
 * @param[in] remote_tuning_active True while the general remote-tuning session is active.
 * @return True when the rendered presentation changed.
 */
bool display_auxiliary_calibration_page_update(DisplayAuxiliaryCalibrationPage *page,
                                               const uint8_t glyphs[3], bool remote_tuning_active) {
    bool changed = page->remote_tuning_active != remote_tuning_active;
    page->remote_tuning_active = remote_tuning_active;
    for (uint8_t index = 0; index < 3; index++) {
        changed |= page->glyphs[index] != glyphs[index];
        page->glyphs[index] = glyphs[index];
    }
    return changed;
}

/**
 * @brief Renders the legacy-monitor opening title.
 *
 * Clears the previous page and draws the inverted title at the official record coordinates.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
void display_auxiliary_calibration_page_render_title(DisplayFramebuffer framebuffer) {
    display_framebuffer_clear(framebuffer);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "Legacy", 0, 12, true);
}

/**
 * @brief Renders the selected legacy display glyphs.
 *
 * Shows three large raw glyphs with their decimal points and the ITM label for an active general
 * remote-tuning session.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Current legacy display mirror.
 */
void display_auxiliary_calibration_page_render(DisplayFramebuffer framebuffer,
                                               const DisplayAuxiliaryCalibrationPage *page) {
    DisplayBitmapQueue queue;
    display_bitmap_queue_reset(&queue);
    display_auxiliary_calibration_page_render_queued(framebuffer, &queue, page);
    display_bitmap_queue_finish(&queue, framebuffer);
}

void display_auxiliary_calibration_page_render_queued(
    DisplayFramebuffer framebuffer, DisplayBitmapQueue *queue,
    const DisplayAuxiliaryCalibrationPage *page) {
    static const uint16_t glyph_x[] = {95, 120, 145};
    display_framebuffer_clear(framebuffer);
    for (uint8_t index = 0; index < 3; index++) {
        draw_glyph(queue, glyph_x[index], page->glyphs[index]);
    }
    if (page->remote_tuning_active) {
        display_text_draw(framebuffer, "ITM", 235, 50, 1, AUXILIARY_CALIBRATION_COLOR);
    }
}
