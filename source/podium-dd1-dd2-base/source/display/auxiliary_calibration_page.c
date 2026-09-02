#include "display/auxiliary_calibration_page.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"

/**
 * @brief Defines legacy-display colors and seven-segment bit assignments.
 *
 * The bit assignments match the raw glyph values supplied by the wheel-service display output.
 */
enum {
    AUXILIARY_CALIBRATION_COLOR = 15, /**< Foreground grayscale value for legacy display content. */
    AUXILIARY_CALIBRATION_SEGMENT_TOP = 1 << 0,         /**< Top segment bit. */
    AUXILIARY_CALIBRATION_SEGMENT_UPPER_RIGHT = 1 << 1, /**< Upper-right segment bit. */
    AUXILIARY_CALIBRATION_SEGMENT_LOWER_RIGHT = 1 << 2, /**< Lower-right segment bit. */
    AUXILIARY_CALIBRATION_SEGMENT_BOTTOM = 1 << 3,      /**< Bottom segment bit. */
    AUXILIARY_CALIBRATION_SEGMENT_LOWER_LEFT = 1 << 4,  /**< Lower-left segment bit. */
    AUXILIARY_CALIBRATION_SEGMENT_UPPER_LEFT = 1 << 5,  /**< Upper-left segment bit. */
    AUXILIARY_CALIBRATION_SEGMENT_MIDDLE = 1 << 6,      /**< Middle segment bit. */
    AUXILIARY_CALIBRATION_DECIMAL_POINT = 1 << 7,       /**< Decimal-point bit. */
    AUXILIARY_CALIBRATION_GLYPH_Y = 18,                 /**< Glyph top coordinate. */
};

/**
 * @brief Draws one filled diagnostic rectangle.
 *
 * Fills the inclusive coordinate range with the requested grayscale value.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] left Left coordinate.
 * @param[in] top Top coordinate.
 * @param[in] right Right coordinate.
 * @param[in] bottom Bottom coordinate.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_rectangle(DisplayFramebuffer framebuffer, uint16_t left, uint16_t top,
                           uint16_t right, uint16_t bottom, uint8_t color) {
    for (uint16_t y = top; y <= bottom; y++) {
        for (uint16_t x = left; x <= right; x++) {
            display_framebuffer_set_pixel(framebuffer, x, y, color);
        }
    }
}

/**
 * @brief Draws one raw seven-segment wheel glyph.
 *
 * Maps segment bits zero through six to the standard bars and bit seven to the decimal point.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Left coordinate of the glyph.
 * @param[in] glyph Raw seven-segment glyph and decimal-point bit.
 */
static void draw_glyph(DisplayFramebuffer framebuffer, uint16_t x, uint8_t glyph) {
    if ((glyph & AUXILIARY_CALIBRATION_SEGMENT_TOP) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 3), AUXILIARY_CALIBRATION_GLYPH_Y,
                       (uint16_t)(x + 18), AUXILIARY_CALIBRATION_GLYPH_Y + 2,
                       AUXILIARY_CALIBRATION_COLOR);
    }
    if ((glyph & AUXILIARY_CALIBRATION_SEGMENT_UPPER_RIGHT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 16), AUXILIARY_CALIBRATION_GLYPH_Y + 3,
                       (uint16_t)(x + 19), AUXILIARY_CALIBRATION_GLYPH_Y + 16,
                       AUXILIARY_CALIBRATION_COLOR);
    }
    if ((glyph & AUXILIARY_CALIBRATION_SEGMENT_LOWER_RIGHT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 16), AUXILIARY_CALIBRATION_GLYPH_Y + 20,
                       (uint16_t)(x + 19), AUXILIARY_CALIBRATION_GLYPH_Y + 33,
                       AUXILIARY_CALIBRATION_COLOR);
    }
    if ((glyph & AUXILIARY_CALIBRATION_SEGMENT_BOTTOM) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 3), AUXILIARY_CALIBRATION_GLYPH_Y + 34,
                       (uint16_t)(x + 18), AUXILIARY_CALIBRATION_GLYPH_Y + 36,
                       AUXILIARY_CALIBRATION_COLOR);
    }
    if ((glyph & AUXILIARY_CALIBRATION_SEGMENT_LOWER_LEFT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 1), AUXILIARY_CALIBRATION_GLYPH_Y + 20,
                       (uint16_t)(x + 4), AUXILIARY_CALIBRATION_GLYPH_Y + 33,
                       AUXILIARY_CALIBRATION_COLOR);
    }
    if ((glyph & AUXILIARY_CALIBRATION_SEGMENT_UPPER_LEFT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 1), AUXILIARY_CALIBRATION_GLYPH_Y + 3,
                       (uint16_t)(x + 4), AUXILIARY_CALIBRATION_GLYPH_Y + 16,
                       AUXILIARY_CALIBRATION_COLOR);
    }
    if ((glyph & AUXILIARY_CALIBRATION_SEGMENT_MIDDLE) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 3), AUXILIARY_CALIBRATION_GLYPH_Y + 17,
                       (uint16_t)(x + 18), AUXILIARY_CALIBRATION_GLYPH_Y + 19,
                       AUXILIARY_CALIBRATION_COLOR);
    }
    if ((glyph & AUXILIARY_CALIBRATION_DECIMAL_POINT) != 0) {
        draw_rectangle(framebuffer, (uint16_t)(x + 21), AUXILIARY_CALIBRATION_GLYPH_Y + 33,
                       (uint16_t)(x + 24), AUXILIARY_CALIBRATION_GLYPH_Y + 36,
                       AUXILIARY_CALIBRATION_COLOR);
    }
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
    static const uint16_t glyph_x[] = {95, 120, 145};
    display_framebuffer_clear(framebuffer);
    for (uint8_t index = 0; index < 3; index++) {
        draw_glyph(framebuffer, glyph_x[index], page->glyphs[index]);
    }
    if (page->remote_tuning_active) {
        display_text_draw(framebuffer, "ITM", 235, 50, 1, AUXILIARY_CALIBRATION_COLOR);
    }
}
