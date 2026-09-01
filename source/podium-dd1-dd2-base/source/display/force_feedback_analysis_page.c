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
    ANALYSIS_SAMPLE_INTERVAL_MS = 25,          /**< Set-point sampling interval in milliseconds. */
    ANALYSIS_COLOR = 15,                       /**< Foreground grayscale value. */
    ANALYSIS_SERIES_COLOR = 8,                 /**< History-series grayscale value. */
    ANALYSIS_GRID_COLOR = 1,                   /**< Chart-grid grayscale value. */
    ANALYSIS_BAR_BORDER_COLOR = 10,            /**< Level-bar border grayscale value. */
    ANALYSIS_BAR_FILL_COLOR = 6,               /**< Level-bar fill grayscale value. */
    ANALYSIS_CHART_LEFT = 5,                   /**< Chart left coordinate. */
    ANALYSIS_CHART_RIGHT = 203,                /**< Chart right coordinate. */
    ANALYSIS_CHART_TOP = 22,                   /**< Chart top coordinate. */
    ANALYSIS_CHART_BOTTOM = 62,                /**< Chart bottom coordinate. */
    ANALYSIS_BAR_TOP = 22,                     /**< Level-bar top coordinate. */
    ANALYSIS_BAR_BOTTOM = 62,                  /**< Level-bar bottom coordinate. */
    ANALYSIS_COUNTERCLOCKWISE_BAR_LEFT = 210,  /**< Counterclockwise bar left coordinate. */
    ANALYSIS_COUNTERCLOCKWISE_BAR_RIGHT = 223, /**< Counterclockwise bar right coordinate. */
    ANALYSIS_CLOCKWISE_BAR_LEFT = 226,         /**< Clockwise bar left coordinate. */
    ANALYSIS_CLOCKWISE_BAR_RIGHT = 239,        /**< Clockwise bar right coordinate. */
    ANALYSIS_TITLE_Y = 28,                     /**< Opening-title top coordinate. */
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
 * @brief Draws one vertical line.
 *
 * Fills every drawable pixel between the two inclusive vertical coordinates.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Horizontal line position.
 * @param[in] first_y First vertical coordinate.
 * @param[in] last_y Last vertical coordinate.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_vertical(DisplayFramebuffer framebuffer, uint16_t x, uint16_t first_y,
                          uint16_t last_y, uint8_t color) {
    for (uint16_t y = first_y; y <= last_y; y++) {
        display_framebuffer_set_pixel(framebuffer, x, y, color);
    }
}

/**
 * @brief Draws one horizontal line.
 *
 * Fills every drawable pixel between the two inclusive horizontal coordinates.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] first_x First horizontal coordinate.
 * @param[in] last_x Last horizontal coordinate.
 * @param[in] y Vertical line position.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_horizontal(DisplayFramebuffer framebuffer, uint16_t first_x, uint16_t last_x,
                            uint16_t y, uint8_t color) {
    for (uint16_t x = first_x; x <= last_x; x++) {
        display_framebuffer_set_pixel(framebuffer, x, y, color);
    }
}

/**
 * @brief Draws the five-second analysis grid.
 *
 * Draws regularly spaced time and percentage divisions across the 200-sample plot, then emphasizes
 * the chart boundary.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_chart_grid(DisplayFramebuffer framebuffer) {
    for (uint16_t x = ANALYSIS_CHART_LEFT; x <= ANALYSIS_CHART_RIGHT; x += 20) {
        draw_vertical(framebuffer, x, ANALYSIS_CHART_TOP, ANALYSIS_CHART_BOTTOM,
                      ANALYSIS_GRID_COLOR);
    }
    for (uint16_t y = ANALYSIS_CHART_TOP; y <= ANALYSIS_CHART_BOTTOM; y += 8) {
        draw_horizontal(framebuffer, ANALYSIS_CHART_LEFT, ANALYSIS_CHART_RIGHT, y,
                        ANALYSIS_GRID_COLOR);
    }
    draw_vertical(framebuffer, ANALYSIS_CHART_LEFT, ANALYSIS_CHART_TOP, ANALYSIS_CHART_BOTTOM,
                  ANALYSIS_COLOR);
    draw_horizontal(framebuffer, ANALYSIS_CHART_LEFT, ANALYSIS_CHART_RIGHT, ANALYSIS_CHART_BOTTOM,
                    ANALYSIS_COLOR);
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
    uint16_t oldest =
        page->sample_count < DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT ? 0 : page->next_sample;
    uint16_t first_x = (uint16_t)(ANALYSIS_CHART_RIGHT - page->sample_count + 1u);
    uint16_t previous_y = ANALYSIS_CHART_BOTTOM;
    for (uint16_t offset = 0; offset < page->sample_count; offset++) {
        uint16_t index =
            (uint16_t)((oldest + offset) % DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT);
        uint16_t y =
            (uint16_t)(ANALYSIS_CHART_BOTTOM -
                       page->samples[index] * (ANALYSIS_CHART_BOTTOM - ANALYSIS_CHART_TOP) / 100u);
        uint16_t first_y = offset == 0 || y < previous_y ? y : previous_y;
        uint16_t last_y = offset == 0 || y > previous_y ? y : previous_y;
        draw_vertical(framebuffer, (uint16_t)(first_x + offset), first_y, last_y,
                      ANALYSIS_SERIES_COLOR);
        previous_y = y;
    }
}

/**
 * @brief Draws one direction level bar.
 *
 * Draws the diagnostic border and fills the selected percentage upward from the bottom edge.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] left Left border coordinate.
 * @param[in] right Right border coordinate.
 * @param[in] percentage Filled percentage from zero through 99.
 */
static void draw_level_bar(DisplayFramebuffer framebuffer, uint16_t left, uint16_t right,
                           uint8_t percentage) {
    draw_horizontal(framebuffer, left, right, ANALYSIS_BAR_TOP, ANALYSIS_BAR_BORDER_COLOR);
    draw_horizontal(framebuffer, left, right, ANALYSIS_BAR_BOTTOM, ANALYSIS_BAR_BORDER_COLOR);
    draw_vertical(framebuffer, left, ANALYSIS_BAR_TOP, ANALYSIS_BAR_BOTTOM,
                  ANALYSIS_BAR_BORDER_COLOR);
    draw_vertical(framebuffer, right, ANALYSIS_BAR_TOP, ANALYSIS_BAR_BOTTOM,
                  ANALYSIS_BAR_BORDER_COLOR);
    uint16_t fill_height =
        (uint16_t)((ANALYSIS_BAR_BOTTOM - ANALYSIS_BAR_TOP - 1u) * percentage / 100u);
    for (uint16_t x = left + 1u; x < right; x++) {
        draw_vertical(framebuffer, x, (uint16_t)(ANALYSIS_BAR_BOTTOM - fill_height),
                      ANALYSIS_BAR_BOTTOM - 1u, ANALYSIS_BAR_FILL_COLOR);
    }
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
 * zero-through-99 percentage, retains it in a 200-sample five-second history, and records zero as
 * counterclockwise and nonzero as clockwise direction.
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
    page->samples[page->next_sample] = page->percentage;
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
 * Clears the previous page and centers the title presented for the first second.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
void display_force_feedback_analysis_page_render_title(DisplayFramebuffer framebuffer) {
    display_framebuffer_clear(framebuffer);
    display_text_draw_centered(framebuffer, "Force Feedback Analysis Screen", ANALYSIS_TITLE_Y, 1,
                               ANALYSIS_COLOR);
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
    display_text_draw(framebuffer, "FFB Set Point:", 4, 2, 1, ANALYSIS_COLOR);
    display_text_draw(framebuffer, percentage, 90, 2, 1, ANALYSIS_COLOR);
    display_text_draw(framebuffer, page->direction == 0 ? "CCW" : "CW", 150, 2, 1, ANALYSIS_COLOR);
    draw_chart_grid(framebuffer);
    draw_samples(framebuffer, page);
    draw_level_bar(framebuffer, ANALYSIS_COUNTERCLOCKWISE_BAR_LEFT,
                   ANALYSIS_COUNTERCLOCKWISE_BAR_RIGHT,
                   page->direction == 0 ? page->percentage : 0);
    draw_level_bar(framebuffer, ANALYSIS_CLOCKWISE_BAR_LEFT, ANALYSIS_CLOCKWISE_BAR_RIGHT,
                   page->direction == 0 ? 0 : page->percentage);
}
