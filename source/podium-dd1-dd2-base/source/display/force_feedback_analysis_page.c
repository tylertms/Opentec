#include "display/force_feedback_analysis_page.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"

/**
 * @brief Defines force-feedback analysis timing, colors, and layout.
 *
 * These constants describe the sample cadence, chart and level-bar geometry, and fixed display
 * positions used by the force-feedback analysis page.
 */
enum {
    ANALYSIS_SAMPLE_INTERVAL_MS = 25, /**< Set-point sampling interval in milliseconds. */
    ANALYSIS_COLOR = 15,              /**< Foreground grayscale value. */
    ANALYSIS_SERIES_COLOR = 8,        /**< History-series grayscale value. */
    ANALYSIS_GRID_COLOR = 1,          /**< Chart-grid grayscale value. */
    ANALYSIS_BAR_BORDER_COLOR = 10,   /**< Level-bar border grayscale value. */
    ANALYSIS_BAR_FILL_COLOR = 6,      /**< Level-bar fill grayscale value. */
    ANALYSIS_CHART_LEFT = 5,          /**< Chart left coordinate. */
    ANALYSIS_CHART_WIDTH = 200,       /**< Chart width in pixels. */
    ANALYSIS_CHART_RIGHT = ANALYSIS_CHART_LEFT + ANALYSIS_CHART_WIDTH,
    /**< Chart right coordinate. */
    ANALYSIS_CHART_HEIGHT = 40,                /**< Chart height in pixels. */
    ANALYSIS_CHART_TOP = 22,                   /**< Chart top coordinate. */
    ANALYSIS_CHART_BOTTOM = 62,                /**< Chart bottom coordinate. */
    ANALYSIS_BAR_TOP = 22,                     /**< Level-bar top coordinate. */
    ANALYSIS_BAR_BOTTOM = 62,                  /**< Level-bar bottom coordinate. */
    ANALYSIS_COUNTERCLOCKWISE_BAR_LEFT = 210,  /**< Counterclockwise bar left coordinate. */
    ANALYSIS_COUNTERCLOCKWISE_BAR_RIGHT = 225, /**< Counterclockwise bar right coordinate. */
    ANALYSIS_CLOCKWISE_BAR_LEFT = 225,         /**< Clockwise bar left coordinate. */
    ANALYSIS_CLOCKWISE_BAR_RIGHT = 240,        /**< Clockwise bar right coordinate. */
    ANALYSIS_TITLE_Y = 12,                     /**< Opening-title top coordinate. */
};

/**
 * @brief Tests whether the next analysis sample is due.
 *
 * Uses signed modular subtraction so the sampling cadence remains valid across timer wrap.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Next sampling deadline.
 * @return True when the sampling deadline is current or past.
 */
static bool sample_due(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief Appends an unsigned percentage value.
 *
 * Emits the shortest decimal representation followed by a percent sign.
 *
 * @param[out] output Null-terminated percentage text.
 * @param[in] percentage Percentage from zero through 99.
 */
static void format_percentage(char output[5], uint8_t percentage) {
    uint8_t index = 0;
    if (percentage >= 10) {
        output[index++] = (char)('0' + percentage / 10u);
    }
    output[index++] = (char)('0' + percentage % 10u);
    output[index++] = '%';
    output[index] = '\0';
}

/**
 * @brief Draws an exclusive-end horizontal display span.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] first_x First column.
 * @param[in] end_x Exclusive end column.
 * @param[in] y Row.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_horizontal_span(DisplayFramebuffer framebuffer, uint16_t first_x, uint16_t end_x,
                                 uint16_t y, uint8_t color) {
    for (uint16_t x = first_x; x < end_x; x++) {
        display_framebuffer_set_pixel(framebuffer, x, y, color);
    }
}

/**
 * @brief Draws an exclusive-end vertical display span.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Column.
 * @param[in] first_y First row.
 * @param[in] end_y Exclusive end row.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_vertical_span(DisplayFramebuffer framebuffer, uint16_t x, uint16_t first_y,
                               uint16_t end_y, uint8_t color) {
    for (uint16_t y = first_y; y < end_y; y++) {
        display_framebuffer_set_pixel(framebuffer, x, y, color);
    }
}

/**
 * @brief Draws one line between two display pixels.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] start_x Starting column.
 * @param[in] start_y Starting row.
 * @param[in] end_x Ending column.
 * @param[in] end_y Ending row.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_line(DisplayFramebuffer framebuffer, uint16_t start_x, uint16_t start_y,
                      uint16_t end_x, uint16_t end_y, uint8_t color) {
    int32_t x = start_x;
    int32_t y = start_y;
    int32_t delta_x = end_x - start_x;
    int32_t delta_y = end_y - start_y;
    int32_t step_x = delta_x < 0 ? -1 : 1;
    int32_t step_y = delta_y < 0 ? -1 : 1;
    if (delta_x < 0) {
        delta_x = -delta_x;
    }
    if (delta_y < 0) {
        delta_y = -delta_y;
    }
    int32_t error = delta_x > delta_y ? delta_x / 2 : -delta_y / 2;
    for (;;) {
        display_framebuffer_set_pixel(framebuffer, (uint16_t)x, (uint16_t)y, color);
        if (x == end_x && y == end_y) {
            return;
        }
        int32_t previous_error = error;
        if (previous_error > -delta_x) {
            error -= delta_y;
            x += step_x;
        }
        if (previous_error < delta_y) {
            error += delta_x;
            y += step_y;
        }
    }
}

/**
 * @brief Draws the five-second analysis grid.
 *
 * Draws regularly spaced time and percentage divisions across the 200-sample plot.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_chart_grid(DisplayFramebuffer framebuffer) {
    for (uint8_t index = 1; index < 5; index++) {
        uint16_t y = (uint16_t)(ANALYSIS_CHART_BOTTOM -
                                (uint16_t)((uint32_t)index * ANALYSIS_CHART_HEIGHT / 5u));
        draw_horizontal_span(framebuffer, ANALYSIS_CHART_LEFT + 1, ANALYSIS_CHART_RIGHT - 1, y,
                             ANALYSIS_GRID_COLOR);
    }
    for (uint8_t index = 1; index < 10; index++) {
        uint16_t x = (uint16_t)(ANALYSIS_CHART_LEFT +
                                (uint16_t)((uint32_t)index * ANALYSIS_CHART_WIDTH / 10u));
        draw_vertical_span(framebuffer, x, ANALYSIS_CHART_TOP, ANALYSIS_CHART_BOTTOM,
                           ANALYSIS_GRID_COLOR);
    }
}

/**
 * @brief Draws retained set-point samples in chronological order.
 *
 * Places the newest sample at the right edge and joins adjacent samples within each chart column.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Retained sample history and ring position.
 */
static void draw_samples(DisplayFramebuffer framebuffer,
                         const DisplayForceFeedbackAnalysisPage *page) {
    for (uint16_t index = 0; index < ANALYSIS_CHART_WIDTH - 2; index++) {
        uint16_t sample_index =
            (uint16_t)((page->next_sample + index) % (ANALYSIS_CHART_WIDTH - 1));
        uint16_t start_y = (uint16_t)(ANALYSIS_CHART_BOTTOM - page->samples[sample_index] - 1u);
        uint16_t end_y = (uint16_t)(ANALYSIS_CHART_BOTTOM - page->samples[sample_index + 1u] - 1u);
        draw_line(framebuffer, (uint16_t)(ANALYSIS_CHART_LEFT + index + 1u), start_y,
                  (uint16_t)(ANALYSIS_CHART_LEFT + index + 2u), end_y, ANALYSIS_SERIES_COLOR);
    }
}

/**
 * @brief Draws a reference progress record.
 *
 * Supports the vertical modes used by the direction bars and the reference split-color overlay.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] left Left record coordinate.
 * @param[in] top Top record coordinate.
 * @param[in] right Exclusive right record coordinate.
 * @param[in] bottom Bottom record coordinate.
 * @param[in] border_width Border thickness.
 * @param[in] border_color Border grayscale value.
 * @param[in] value_color Fill grayscale value.
 * @param[in] mode Reference fill mode.
 * @param[in] value Fill percentage.
 */
static void draw_progress(DisplayFramebuffer framebuffer, uint16_t left, uint16_t top,
                          uint16_t right, uint16_t bottom, uint8_t border_width,
                          uint8_t border_color, uint8_t value_color, uint8_t mode, uint8_t value) {
    if (border_width != 0) {
        for (uint8_t offset = 0; offset < border_width; offset++) {
            draw_horizontal_span(framebuffer, left, right, (uint16_t)(top + offset), border_color);
            draw_horizontal_span(framebuffer, left, right, (uint16_t)(bottom + offset),
                                 border_color);
            draw_vertical_span(framebuffer, (uint16_t)(left + offset), top, bottom, border_color);
            draw_vertical_span(framebuffer, (uint16_t)(right + offset), top,
                               (uint16_t)(bottom + border_width), border_color);
        }
    }
    if (value > 99) {
        value = 100;
    }
    if (mode == 2 || mode == 3) {
        uint16_t filled = (uint16_t)((bottom - top) * value / 100u);
        if (filled == 0) {
            return;
        }
        uint16_t first_x = left + border_width;
        uint16_t last_x = right - border_width;
        for (uint16_t x = first_x; x < last_x; x++) {
            uint16_t first_y;
            uint16_t last_y;
            if (mode == 2) {
                first_y = (uint16_t)(bottom - filled);
                last_y = bottom;
            } else {
                first_y = (uint16_t)(top + border_width);
                last_y = (uint16_t)(first_y + filled);
            }
            draw_vertical_span(framebuffer, x, first_y, last_y, value_color);
        }
    }
}

/**
 * @brief Draws a split-color reference progress record.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] left Left record coordinate.
 * @param[in] top Top record coordinate.
 * @param[in] right Exclusive right record coordinate.
 * @param[in] bottom Bottom record coordinate.
 * @param[in] mode Reference fill mode.
 * @param[in] percentage Fill percentage.
 */
static void draw_split_progress(DisplayFramebuffer framebuffer, uint16_t left, uint16_t top,
                                uint16_t right, uint16_t bottom, uint8_t mode, uint8_t percentage) {
    draw_progress(framebuffer, left, top, right, bottom, 1, ANALYSIS_BAR_BORDER_COLOR,
                  ANALYSIS_BAR_FILL_COLOR - 4u, mode, percentage);
    uint16_t primary_top = top;
    if (mode == 1 || mode == 3) {
        primary_top++;
    }
    draw_progress(framebuffer, left + 1u, primary_top, right, bottom, 0, 0, ANALYSIS_BAR_FILL_COLOR,
                  mode, percentage);
}

/**
 * @brief Opens a force-feedback analysis session.
 *
 * Preserves retained samples and aligns the next sample with the global 25-millisecond cadence.
 *
 * @param[in,out] page Analysis sample history and current presentation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void display_force_feedback_analysis_page_open(DisplayForceFeedbackAnalysisPage *page,
                                               uint32_t now_ms) {
    page->next_sample_ms =
        now_ms + ANALYSIS_SAMPLE_INTERVAL_MS - now_ms % ANALYSIS_SAMPLE_INTERVAL_MS;
}

/**
 * @brief Samples the force-feedback set point.
 *
 * When the 25-millisecond sampling deadline is due, converts the unsigned 16-bit magnitude to a
 * zero-through-99 percentage and a 0-through-39 chart sample, retains it in a 200-sample
 * five-second history, and records zero as counterclockwise and nonzero as clockwise direction.
 *
 * @param[in,out] page Analysis sample history and current presentation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] direction Zero for counterclockwise; nonzero for clockwise.
 * @param[in] position Unsigned set-point magnitude.
 * @return True when a new sample was retained.
 */
bool display_force_feedback_analysis_page_update(DisplayForceFeedbackAnalysisPage *page,
                                                 uint32_t now_ms, uint8_t direction,
                                                 uint16_t position) {
    if (!sample_due(now_ms, page->next_sample_ms)) {
        return false;
    }
    page->percentage = (uint8_t)(((uint32_t)position * 100u) >> 16);
    page->direction = direction;
    page->samples[page->next_sample] =
        (uint8_t)((uint32_t)(ANALYSIS_CHART_HEIGHT - 1u) * position / UINT16_MAX);
    page->next_sample =
        (uint16_t)((page->next_sample + 1u) % DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT);
    if (page->sample_count < DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT) {
        page->sample_count++;
    }
    page->next_sample_ms = now_ms + ANALYSIS_SAMPLE_INTERVAL_MS;
    return true;
}

/**
 * @brief Renders the force-feedback analysis opening title.
 *
 * Clears the previous page and draws the inverted title at the official record coordinates.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
void display_force_feedback_analysis_page_render_title(DisplayFramebuffer framebuffer) {
    display_framebuffer_clear(framebuffer);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988,
                                "Force Feedback Analysis Screen", 0, ANALYSIS_TITLE_Y, true);
}

/**
 * @brief Renders force-feedback set-point history and direction.
 *
 * Shows the current percentage and CW or CCW direction above a five-second chart and two
 * direction-specific level bars.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Current sample history, magnitude, and direction.
 */
void display_force_feedback_analysis_page_render(DisplayFramebuffer framebuffer,
                                                 const DisplayForceFeedbackAnalysisPage *page) {
    char percentage[5];
    format_percentage(percentage, page->percentage);
    display_framebuffer_clear(framebuffer);
    display_text_draw(framebuffer, "FFB Set Point: ", 5, 12, 1, ANALYSIS_COLOR);
    display_text_draw(framebuffer, percentage, 75, 12, 1, ANALYSIS_COLOR);
    display_text_draw(framebuffer, page->direction == 0 ? "CCW" : "CW", 150, 12, 1, ANALYSIS_COLOR);
    draw_chart_grid(framebuffer);
    draw_samples(framebuffer, page);
    draw_horizontal_span(framebuffer, ANALYSIS_CHART_LEFT, ANALYSIS_CHART_RIGHT,
                         ANALYSIS_CHART_BOTTOM, ANALYSIS_COLOR);
    draw_vertical_span(framebuffer, ANALYSIS_CHART_LEFT, ANALYSIS_CHART_TOP, ANALYSIS_CHART_BOTTOM,
                       ANALYSIS_COLOR);
    draw_split_progress(framebuffer, ANALYSIS_COUNTERCLOCKWISE_BAR_LEFT, ANALYSIS_BAR_TOP,
                        ANALYSIS_COUNTERCLOCKWISE_BAR_RIGHT, ANALYSIS_BAR_BOTTOM, 2,
                        page->direction == 0 ? page->percentage : 0);
    draw_split_progress(framebuffer, ANALYSIS_CLOCKWISE_BAR_LEFT, ANALYSIS_BAR_TOP,
                        ANALYSIS_CLOCKWISE_BAR_RIGHT, ANALYSIS_BAR_BOTTOM, 2,
                        page->direction == 0 ? 0 : page->percentage);
}
