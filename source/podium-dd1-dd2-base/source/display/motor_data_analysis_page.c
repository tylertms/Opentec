#include "display/motor_data_analysis_page.h"

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "display/framebuffer.h"
#include "display/text.h"

/**
 * @brief Defines motor-analysis timing, scaling, colors, and layout.
 *
 * These constants describe the sampling cadence, torque ranges, chart geometry, and fixed
 * framebuffer positions used by the motor-analysis page.
 */
enum {
    MOTOR_ANALYSIS_SAMPLE_INTERVAL_MS = 41, /**< Torque-chart sampling interval in milliseconds. */
    MOTOR_ANALYSIS_PEAK_INTERVAL_MS = 30,   /**< Peak-hold evaluation interval in milliseconds. */
    MOTOR_ANALYSIS_PEAK_HOLD_MS = 10000,    /**< Peak-hold duration in milliseconds. */
    MOTOR_ANALYSIS_DD1_LIMIT = 20000,     /**< DD1 torque limit in thousandths of a newton-metre. */
    MOTOR_ANALYSIS_DD2_LIMIT = 25000,     /**< DD2 torque limit in thousandths of a newton-metre. */
    MOTOR_ANALYSIS_COLOR = 15,            /**< Foreground grayscale value. */
    MOTOR_ANALYSIS_SERIES_COLOR = 8,      /**< History-series grayscale value. */
    MOTOR_ANALYSIS_GRID_COLOR = 1,        /**< Chart-grid grayscale value. */
    MOTOR_ANALYSIS_BAR_BORDER_COLOR = 10, /**< Torque-bar border grayscale value. */
    MOTOR_ANALYSIS_BAR_FILL_COLOR = 6,    /**< Torque-bar fill grayscale value. */
    MOTOR_ANALYSIS_CHART_LEFT = 3,        /**< Chart left coordinate. */
    MOTOR_ANALYSIS_CHART_WIDTH = 120,     /**< Chart width in pixels. */
    MOTOR_ANALYSIS_CHART_RIGHT = MOTOR_ANALYSIS_CHART_LEFT + MOTOR_ANALYSIS_CHART_WIDTH,
    /**< Chart right coordinate. */
    MOTOR_ANALYSIS_CHART_HEIGHT = 40, /**< Chart height in pixels. */
    MOTOR_ANALYSIS_CHART_TOP = 23,    /**< Chart top coordinate. */
    MOTOR_ANALYSIS_CHART_BOTTOM = 63, /**< Chart bottom coordinate. */
    MOTOR_ANALYSIS_SEPARATOR_X = 150, /**< Telemetry-column separator coordinate. */
};

/**
 * @brief Tests whether a motor-analysis deadline is due.
 *
 * Uses signed modular subtraction so sampling and peak hold remain valid across timer wrap.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to test.
 * @return True when the deadline is current or past.
 */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief Returns the absolute magnitude of one signed torque sample.
 *
 * Handles the complete signed 16-bit input range without signed overflow.
 *
 * @param[in] value Signed torque sample.
 * @return Unsigned absolute magnitude.
 */
static uint16_t magnitude(int16_t value) {
    return value < 0 ? (uint16_t)(-(int32_t)value) : (uint16_t)value;
}

/**
 * @brief Appends an unsigned decimal value with optional leading zeroes.
 *
 * Writes at least the requested number of digits and returns the next available character.
 *
 * @param[in,out] output First available output character.
 * @param[in] value Value to append.
 * @param[in] minimum_digits Minimum number of emitted digits.
 * @return First available character after the digits.
 */
static char *append_unsigned(char *output, uint32_t value, uint8_t minimum_digits) {
    uint32_t divisor = 1;
    uint8_t digits = 1;
    while (value / divisor >= 10u) {
        divisor *= 10u;
        digits++;
    }
    while (digits < minimum_digits) {
        divisor *= 10u;
        digits++;
    }
    do {
        *output++ = (char)('0' + value / divisor % 10u);
        divisor /= 10u;
    } while (divisor != 0);
    return output;
}

/**
 * @brief Formats torque with one decimal place in newton-metres.
 *
 * Converts thousandths of a newton-metre to rounded tenths and appends the local unit suffix.
 *
 * @param[out] output Null-terminated torque text.
 * @param[in] torque Signed torque in thousandths of a newton-metre.
 */
static void format_torque(char output[16], int16_t torque) {
    uint16_t tenths = (uint16_t)((magnitude(torque) + 50u) / 100u);
    char *cursor = output;
    if (torque < 0 && tenths != 0) {
        *cursor++ = '-';
    }
    cursor = append_unsigned(cursor, tenths / 10u, 1);
    *cursor++ = '.';
    *cursor++ = (char)('0' + tenths % 10u);
    *cursor++ = ' ';
    *cursor++ = 'N';
    *cursor++ = 'm';
    *cursor = '\0';
}

/**
 * @brief Formats a signed temperature in degrees Celsius.
 *
 * Emits the shortest signed decimal representation and appends the local degree marker.
 *
 * @param[out] output Null-terminated temperature text.
 * @param[in] temperature Signed temperature in degrees Celsius.
 */
static void format_temperature(char output[16], int16_t temperature) {
    char *cursor = output;
    if (temperature < 0) {
        *cursor++ = '-';
    }
    cursor = append_unsigned(cursor, magnitude(temperature), 1);
    *cursor++ = ' ';
    *cursor++ = '`';
    *cursor++ = 'C';
    *cursor = '\0';
}

/**
 * @brief Formats fan speed with four minimum decimal digits.
 *
 * Appends the RPM suffix after the zero-padded tachometer value.
 *
 * @param[out] output Null-terminated fan-speed text.
 * @param[in] fan_speed_rpm Fan speed in revolutions per minute.
 */
static void format_fan_speed(char output[16], uint16_t fan_speed_rpm) {
    char *cursor = append_unsigned(output, fan_speed_rpm, 4);
    *cursor++ = ' ';
    *cursor++ = 'R';
    *cursor++ = 'P';
    *cursor++ = 'M';
    *cursor = '\0';
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
 * @brief Draws the signed five-second torque chart.
 *
 * Renders the approximately five-second chart, six torque divisions, and retained samples in
 * chronological order with zero torque at the chart midpoint.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Retained scaled torque samples and ring position.
 */
static void draw_chart(DisplayFramebuffer framebuffer, const DisplayMotorDataAnalysisPage *page) {
    for (uint8_t index = 1; index < 6; index++) {
        uint16_t y = (uint16_t)(MOTOR_ANALYSIS_CHART_BOTTOM -
                                (uint16_t)((uint32_t)index * MOTOR_ANALYSIS_CHART_HEIGHT / 6u));
        draw_horizontal_span(framebuffer, MOTOR_ANALYSIS_CHART_LEFT + 1,
                             MOTOR_ANALYSIS_CHART_RIGHT - 1, y, MOTOR_ANALYSIS_GRID_COLOR);
    }
    for (uint8_t index = 1; index < 5; index++) {
        uint16_t x = (uint16_t)(MOTOR_ANALYSIS_CHART_LEFT +
                                (uint16_t)((uint32_t)index * MOTOR_ANALYSIS_CHART_WIDTH / 5u));
        draw_vertical_span(framebuffer, x, MOTOR_ANALYSIS_CHART_TOP, MOTOR_ANALYSIS_CHART_BOTTOM,
                           MOTOR_ANALYSIS_GRID_COLOR);
    }
    for (uint16_t index = 0; index < MOTOR_ANALYSIS_CHART_WIDTH - 2; index++) {
        uint16_t sample_index =
            (uint16_t)((page->next_sample + index) % (MOTOR_ANALYSIS_CHART_WIDTH - 1));
        uint16_t start_y =
            (uint16_t)(MOTOR_ANALYSIS_CHART_BOTTOM - page->samples[sample_index] - 1u);
        uint16_t end_y =
            (uint16_t)(MOTOR_ANALYSIS_CHART_BOTTOM - page->samples[sample_index + 1u] - 1u);
        draw_line(framebuffer, (uint16_t)(MOTOR_ANALYSIS_CHART_LEFT + index + 1u), start_y,
                  (uint16_t)(MOTOR_ANALYSIS_CHART_LEFT + index + 2u), end_y,
                  MOTOR_ANALYSIS_SERIES_COLOR);
    }
    draw_vertical_span(framebuffer, MOTOR_ANALYSIS_CHART_LEFT, MOTOR_ANALYSIS_CHART_TOP,
                       MOTOR_ANALYSIS_CHART_BOTTOM, MOTOR_ANALYSIS_COLOR);
    draw_horizontal_span(framebuffer, MOTOR_ANALYSIS_CHART_LEFT, MOTOR_ANALYSIS_CHART_RIGHT,
                         MOTOR_ANALYSIS_CHART_BOTTOM, MOTOR_ANALYSIS_COLOR);
}

/**
 * @brief Draws one reference progress record.
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
    uint16_t filled = (uint16_t)((bottom - top) * value / 100u);
    if (filled == 0) {
        return;
    }
    uint16_t first_x = left + border_width;
    uint16_t last_x = right - border_width;
    for (uint16_t x = first_x; x < last_x; x++) {
        uint16_t first_y = mode == 2 ? (uint16_t)(bottom - filled) : (uint16_t)(top + border_width);
        uint16_t last_y = mode == 2 ? bottom : (uint16_t)(first_y + filled);
        draw_vertical_span(framebuffer, x, first_y, last_y, value_color);
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
    draw_progress(framebuffer, left, top, right, bottom, 1, MOTOR_ANALYSIS_BAR_BORDER_COLOR,
                  MOTOR_ANALYSIS_BAR_FILL_COLOR - 4u, mode, percentage);
    uint16_t primary_top = top;
    if (mode == 1 || mode == 3) {
        primary_top++;
    }
    draw_progress(framebuffer, left + 1u, primary_top, right, bottom, 0, 0,
                  MOTOR_ANALYSIS_BAR_FILL_COLOR, mode, percentage);
}

/**
 * @brief Draws positive and negative torque level bars.
 *
 * Scales the current torque against the DD1 or DD2 range and fills only the matching half.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Current torque and variant-specific limit.
 */
static void draw_torque_bars(DisplayFramebuffer framebuffer,
                             const DisplayMotorDataAnalysisPage *page) {
    uint16_t percentage = (uint16_t)((uint32_t)magnitude(page->torque) * 100u / page->torque_limit);
    if (percentage > 99) {
        percentage = 99;
    }
    draw_split_progress(framebuffer, 137, 24, 143, 43, 2,
                        page->torque >= 0 ? (uint8_t)percentage : 0);
    draw_split_progress(framebuffer, 137, 43, 143, 62, 3,
                        page->torque < 0 ? (uint8_t)percentage : 0);
}

/**
 * @brief Opens the motor-data analyzer.
 *
 * Preserves torque history and peak hold, selects the DD1 or DD2 torque range, and aligns chart
 * sampling with the next global 41-millisecond tick.
 *
 * @param[in,out] page Retained motor-analysis state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] variant Hardware-selected base variant.
 */
void display_motor_data_analysis_page_open(DisplayMotorDataAnalysisPage *page, uint32_t now_ms,
                                           BoardVariant variant) {
    page->torque_limit =
        variant == BOARD_VARIANT_DD1 ? MOTOR_ANALYSIS_DD1_LIMIT : MOTOR_ANALYSIS_DD2_LIMIT;
    page->next_sample_ms =
        now_ms + MOTOR_ANALYSIS_SAMPLE_INTERVAL_MS - now_ms % MOTOR_ANALYSIS_SAMPLE_INTERVAL_MS;
}

/**
 * @brief Updates motor analysis from live diagnostics.
 *
 * Samples the five-second torque chart every 41 milliseconds, evaluates absolute peak torque every
 * 30 milliseconds with a ten-second hold, and retains changed temperatures and the display fan
 * tachometer.
 *
 * @param[in,out] page Retained motor-analysis state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] torque Signed torque in thousandths of a newton-metre.
 * @param[in] motor_temperature Motor temperature in degrees Celsius.
 * @param[in] driver_temperature Driver temperature in degrees Celsius.
 * @param[in] fan_speed_rpm Display fan tachometer speed in revolutions per minute.
 * @return True when displayed motor data changed.
 */
bool display_motor_data_analysis_page_update(DisplayMotorDataAnalysisPage *page, uint32_t now_ms,
                                             int16_t torque, int16_t motor_temperature,
                                             int16_t driver_temperature, uint16_t fan_speed_rpm) {
    bool changed = false;
    if (deadline_reached(now_ms, page->next_sample_ms)) {
        page->torque = torque;
        int32_t clamped = torque;
        if (clamped > (int32_t)page->torque_limit) {
            clamped = page->torque_limit;
        } else if (clamped < -(int32_t)page->torque_limit) {
            clamped = -(int32_t)page->torque_limit;
        }
        page->samples[page->next_sample] =
            (uint8_t)((clamped + page->torque_limit) * 39 / ((int32_t)page->torque_limit * 2));
        page->next_sample =
            (uint16_t)((page->next_sample + 1u) % DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT);
        if (page->sample_count < DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT) {
            page->sample_count++;
        }
        page->next_sample_ms = now_ms + MOTOR_ANALYSIS_SAMPLE_INTERVAL_MS;
        changed = true;
    }

    if (deadline_reached(now_ms, page->next_peak_update_ms)) {
        page->next_peak_update_ms = now_ms + MOTOR_ANALYSIS_PEAK_INTERVAL_MS;
        uint16_t current_magnitude = magnitude(torque);
        if (current_magnitude > page->peak_magnitude) {
            page->peak_torque = torque;
            page->peak_magnitude = current_magnitude;
            page->peak_expiry_ms = now_ms + MOTOR_ANALYSIS_PEAK_HOLD_MS;
            changed = true;
        } else if (deadline_reached(now_ms, page->peak_expiry_ms)) {
            changed |= page->peak_torque != torque;
            page->peak_torque = torque;
            page->peak_magnitude = current_magnitude;
        }
    }

    if (page->motor_temperature != motor_temperature ||
        page->driver_temperature != driver_temperature || page->fan_speed_rpm != fan_speed_rpm) {
        page->motor_temperature = motor_temperature;
        page->driver_temperature = driver_temperature;
        page->fan_speed_rpm = fan_speed_rpm;
        changed = true;
    }
    return changed;
}

/**
 * @brief Renders the motor-data analysis opening title.
 *
 * Clears the previous page and draws the inverted title at the official record coordinates.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
void display_motor_data_analysis_page_render_title(DisplayFramebuffer framebuffer) {
    display_framebuffer_clear(framebuffer);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "Motor Data Analysis Screen",
                                0, 12, true);
}

/**
 * @brief Renders live motor torque and supporting telemetry.
 *
 * Shows current and ten-second peak torque, a signed five-second chart, directional level bars,
 * motor and driver temperatures, and the display fan tachometer.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Current motor-analysis state.
 */
void display_motor_data_analysis_page_render(DisplayFramebuffer framebuffer,
                                             const DisplayMotorDataAnalysisPage *page) {
    char value[16];
    display_framebuffer_clear(framebuffer);
    display_text_draw(framebuffer, "M:", 1, 13, 1, MOTOR_ANALYSIS_COLOR);
    format_torque(value, page->torque);
    display_text_draw(framebuffer, value, 15, 13, 1, MOTOR_ANALYSIS_COLOR);
    display_text_draw(framebuffer, "Mpk(10s):", 64, 13, 1, MOTOR_ANALYSIS_COLOR);
    format_torque(value, page->peak_torque);
    display_text_draw(framebuffer, value, 108, 13, 1, MOTOR_ANALYSIS_COLOR);
    draw_chart(framebuffer, page);
    draw_torque_bars(framebuffer, page);
    draw_vertical_span(framebuffer, MOTOR_ANALYSIS_SEPARATOR_X, 13, MOTOR_ANALYSIS_CHART_BOTTOM,
                       MOTOR_ANALYSIS_COLOR);

    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "Motor Data", 151, 13, true);
    display_text_draw(framebuffer, "Fan Speed:", 153, 23, 1, MOTOR_ANALYSIS_COLOR);
    format_fan_speed(value, page->fan_speed_rpm);
    display_text_draw(framebuffer, value, 213, 23, 1, MOTOR_ANALYSIS_COLOR);
    display_text_draw(framebuffer, "Tmp Motor:", 153, 33, 1, MOTOR_ANALYSIS_COLOR);
    format_temperature(value, page->motor_temperature);
    display_text_draw(framebuffer, value, 213, 33, 1, MOTOR_ANALYSIS_COLOR);
    display_text_draw(framebuffer, "Tmp Driver:", 153, 43, 1, MOTOR_ANALYSIS_COLOR);
    format_temperature(value, page->driver_temperature);
    display_text_draw(framebuffer, value, 213, 43, 1, MOTOR_ANALYSIS_COLOR);
}
