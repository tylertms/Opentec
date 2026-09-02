#include "display/temperature_analysis_page.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"

/**
 * @brief Defines temperature-analysis timing, scaling, colors, and chart geometry.
 *
 * These constants describe the sample cadence, 120-degree scale, chart dimensions, and display
 * colors used by the temperature-analysis page.
 */
enum {
    TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS =
        2250, /**< Temperature-chart sampling interval in milliseconds. */
    TEMPERATURE_ANALYSIS_UPPER_LIMIT =
        120, /**< Upper chart temperature limit in degrees Celsius. */
    TEMPERATURE_ANALYSIS_CHART_HEIGHT = 30, /**< Chart height in pixels. */
    TEMPERATURE_ANALYSIS_CHART_WIDTH = 40,  /**< Chart width in pixels. */
    TEMPERATURE_ANALYSIS_CHART_BOTTOM = 58, /**< Chart bottom coordinate. */
    TEMPERATURE_ANALYSIS_COLOR = 15,        /**< Foreground grayscale value. */
    TEMPERATURE_ANALYSIS_SERIES_COLOR = 8,  /**< History-series grayscale value. */
    TEMPERATURE_ANALYSIS_GRID_COLOR = 1,    /**< Chart-grid grayscale value. */
};

/** @brief Left coordinate of each temperature chart in channel order. */
static const uint16_t chart_left[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {25, 68, 110, 152};
/**
 * @brief Tests whether a temperature-analysis deadline is due.
 *
 * Uses signed modular subtraction so the global sampling cadence remains valid across timer wrap.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to test.
 * @return True when the deadline is current or past.
 */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief Returns the absolute magnitude of one signed value.
 *
 * Handles the complete signed 16-bit input range without signed overflow.
 *
 * @param[in] value Signed value.
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
 * @brief Formats one signed temperature.
 *
 * Emits the shortest signed decimal representation without a repeated unit suffix.
 *
 * @param[out] output Null-terminated temperature text.
 * @param[in] temperature Temperature in degrees Celsius.
 */
static void format_temperature(char output[8], int16_t temperature) {
    char *cursor = output;
    if (temperature < 0) {
        *cursor++ = '-';
    }
    cursor = append_unsigned(cursor, magnitude(temperature), 1);
    *cursor = '\0';
}

/**
 * @brief Formats display fan tachometer speed with four minimum digits.
 *
 * Appends the RPM suffix used by the temperature diagnostic.
 *
 * @param[out] output Null-terminated fan-speed text.
 * @param[in] fan_speed_rpm Display fan tachometer speed in revolutions per minute.
 */
static void format_fan_speed(char output[12], uint16_t fan_speed_rpm) {
    char *cursor = append_unsigned(output, fan_speed_rpm, 4);
    *cursor++ = ' ';
    *cursor++ = 'R';
    *cursor++ = 'P';
    *cursor++ = 'M';
    *cursor = '\0';
}

/**
 * @brief Formats thermally available output power.
 *
 * Emits three minimum digits followed by a percent suffix.
 *
 * @param[out] output Null-terminated power text.
 * @param[in] power_percent Available output power in percent.
 */
static void format_power(char output[8], uint8_t power_percent) {
    char *cursor = append_unsigned(output, power_percent, 3);
    *cursor++ = ' ';
    *cursor++ = '%';
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
 * @brief Scales one temperature into the chart height.
 *
 * Clamps the diagnostic range from zero through 120 degrees Celsius to 30 plot rows.
 *
 * @param[in] temperature Temperature in degrees Celsius.
 * @return Scaled chart sample from zero through 29.
 */
static uint8_t scale_temperature(int16_t temperature) {
    if (temperature <= 0) {
        return 0;
    }
    if (temperature >= TEMPERATURE_ANALYSIS_UPPER_LIMIT) {
        return TEMPERATURE_ANALYSIS_CHART_HEIGHT - 1;
    }
    return (uint8_t)((uint16_t)temperature * (TEMPERATURE_ANALYSIS_CHART_HEIGHT - 1) /
                     TEMPERATURE_ANALYSIS_UPPER_LIMIT);
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
 * @brief Draws one retained temperature chart.
 *
 * Renders three 30-second divisions, six temperature divisions, axes, and chronological samples.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Retained temperature-analysis state.
 * @param[in] channel Temperature channel to render.
 */
static void draw_chart(DisplayFramebuffer framebuffer, const DisplayTemperatureAnalysisPage *page,
                       DisplayTemperatureAnalysisChannel channel) {
    uint16_t left = chart_left[channel];
    uint16_t top = TEMPERATURE_ANALYSIS_CHART_BOTTOM - TEMPERATURE_ANALYSIS_CHART_HEIGHT;
    for (uint8_t division = 1; division < 6; division++) {
        uint16_t y =
            (uint16_t)(TEMPERATURE_ANALYSIS_CHART_BOTTOM -
                       (uint16_t)((uint32_t)division * TEMPERATURE_ANALYSIS_CHART_HEIGHT / 6u));
        draw_horizontal_span(framebuffer, (uint16_t)(left + 1),
                             (uint16_t)(left + TEMPERATURE_ANALYSIS_CHART_WIDTH - 1), y,
                             TEMPERATURE_ANALYSIS_GRID_COLOR);
    }
    for (uint8_t division = 1; division < 3; division++) {
        uint16_t x = (uint16_t)(left + (uint16_t)((uint32_t)division *
                                                  TEMPERATURE_ANALYSIS_CHART_WIDTH / 3u));
        draw_vertical_span(framebuffer, x, top, TEMPERATURE_ANALYSIS_CHART_BOTTOM,
                           TEMPERATURE_ANALYSIS_GRID_COLOR);
    }
    for (uint16_t index = 0; index < TEMPERATURE_ANALYSIS_CHART_WIDTH - 2; index++) {
        uint8_t sample_index =
            (uint8_t)((page->next_sample + index) % (TEMPERATURE_ANALYSIS_CHART_WIDTH - 1));
        uint16_t start_y = (uint16_t)(TEMPERATURE_ANALYSIS_CHART_BOTTOM -
                                      page->samples[channel][sample_index] - 1u);
        uint16_t end_y = (uint16_t)(TEMPERATURE_ANALYSIS_CHART_BOTTOM -
                                    page->samples[channel][sample_index + 1u] - 1u);
        draw_line(framebuffer, (uint16_t)(left + index + 1u), start_y,
                  (uint16_t)(left + index + 2u), end_y, TEMPERATURE_ANALYSIS_SERIES_COLOR);
    }
    draw_vertical_span(framebuffer, left, top, TEMPERATURE_ANALYSIS_CHART_BOTTOM,
                       TEMPERATURE_ANALYSIS_COLOR);
    draw_horizontal_span(framebuffer, left, (uint16_t)(left + TEMPERATURE_ANALYSIS_CHART_WIDTH),
                         TEMPERATURE_ANALYSIS_CHART_BOTTOM, TEMPERATURE_ANALYSIS_COLOR);
}

/**
 * @brief Formats one labeled temperature line.
 *
 * @param[out] output Null-terminated labeled temperature text.
 * @param[in] prefix Four-character diagnostic label including its colon.
 * @param[in] temperature Temperature in degrees Celsius.
 */
static void format_temperature_line(char output[10], const char prefix[4], int16_t temperature) {
    char value[16];
    output[0] = prefix[0];
    output[1] = prefix[1];
    output[2] = prefix[2];
    output[3] = prefix[3];
    output[4] = ' ';
    format_temperature(value, temperature);
    uint8_t index = 0;
    while (value[index] != '\0' && index < 4) {
        output[5u + index] = value[index];
        index++;
    }
    output[5u + index] = '\0';
}

/**
 * @brief Opens the temperature analyzer.
 *
 * Preserves all four histories and aligns sampling with the next global 2.25-second tick.
 *
 * @param[in,out] page Retained temperature-analysis state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void display_temperature_analysis_page_open(DisplayTemperatureAnalysisPage *page, uint32_t now_ms) {
    page->next_sample_ms = now_ms + TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS -
                           now_ms % TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS;
}

/**
 * @brief Updates temperature histories and live cooling values.
 *
 * Samples all four 90-second plots every 2.25 seconds and publishes changed fan tachometer speed
 * and thermally available output power without waiting for the chart cadence.
 *
 * @param[in,out] page Retained temperature-analysis state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] temperatures Motor, driver, base, and quick-release temperatures in degrees Celsius.
 * @param[in] fan_speed_rpm Display fan tachometer speed in revolutions per minute.
 * @param[in] power_percent Thermally available output power in percent.
 * @return True when displayed analysis data changed.
 */
bool display_temperature_analysis_page_update(
    DisplayTemperatureAnalysisPage *page, uint32_t now_ms,
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT], uint16_t fan_speed_rpm,
    uint8_t power_percent) {
    bool changed = false;
    if (deadline_reached(now_ms, page->next_sample_ms)) {
        for (uint8_t channel = 0; channel < DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT; channel++) {
            page->temperatures[channel] = temperatures[channel];
            page->samples[channel][page->next_sample] = scale_temperature(temperatures[channel]);
        }
        page->next_sample =
            (uint8_t)((page->next_sample + 1u) % DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT);
        if (page->sample_count < DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT) {
            page->sample_count++;
        }
        page->next_sample_ms = now_ms + TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS;
        changed = true;
    }
    if (page->fan_speed_rpm != fan_speed_rpm || page->power_percent != power_percent) {
        page->fan_speed_rpm = fan_speed_rpm;
        page->power_percent = power_percent;
        changed = true;
    }
    return changed;
}

/**
 * @brief Renders the temperature-analysis opening title.
 *
 * Clears the previous page and draws the inverted title at the official record coordinates.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
void display_temperature_analysis_page_render_title(DisplayFramebuffer framebuffer) {
    display_framebuffer_clear(framebuffer);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "Temperature Analysis Screen",
                                0, 12, true);
}

/**
 * @brief Renders temperature history, fan speed, and available output power.
 *
 * Shows four labeled 90-second temperature plots, display fan tachometer RPM, and the current
 * thermal power allowance.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Current temperature-analysis state.
 */
void display_temperature_analysis_page_render(DisplayFramebuffer framebuffer,
                                              const DisplayTemperatureAnalysisPage *page) {
    char value[12];
    static const char labels[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT][4] = {
        {'M', 'o', 't', ':'}, {'D', 'r', 'v', ':'}, {'B', 'a', 's', ':'}, {'W', 'Q', 'R', ':'}};
    display_framebuffer_clear(framebuffer);
    display_text_draw(framebuffer, "[`C]", 2, 13, 1, TEMPERATURE_ANALYSIS_COLOR);

    for (uint8_t channel = 0; channel < DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT; channel++) {
        format_temperature_line(value, labels[channel], page->temperatures[channel]);
        display_text_draw(framebuffer, value, chart_left[channel], 15, 1,
                          TEMPERATURE_ANALYSIS_COLOR);
        draw_chart(framebuffer, page, (DisplayTemperatureAnalysisChannel)channel);
    }

    draw_vertical_span(framebuffer, 199, 13, 41, TEMPERATURE_ANALYSIS_COLOR);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "Fan", 200, 13, true);
    format_fan_speed(value, page->fan_speed_rpm);
    display_text_draw(framebuffer, value, 202, 23, 1, TEMPERATURE_ANALYSIS_COLOR);
    draw_vertical_span(framebuffer, 199, 43, 61, TEMPERATURE_ANALYSIS_COLOR);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "Power", 200, 43, true);
    format_power(value, page->power_percent);
    display_text_draw(framebuffer, value, 202, 53, 1, TEMPERATURE_ANALYSIS_COLOR);
}
