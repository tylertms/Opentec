#include "display/motor_data_analysis_page.h"

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "display/framebuffer.h"
#include "display/text.h"

enum {
    MOTOR_ANALYSIS_SAMPLE_INTERVAL_MS = 41,
    MOTOR_ANALYSIS_PEAK_INTERVAL_MS = 30,
    MOTOR_ANALYSIS_PEAK_HOLD_MS = 10000,
    MOTOR_ANALYSIS_DD1_LIMIT = 20000,
    MOTOR_ANALYSIS_DD2_LIMIT = 25000,
    MOTOR_ANALYSIS_COLOR = 15,
    MOTOR_ANALYSIS_SERIES_COLOR = 8,
    MOTOR_ANALYSIS_GRID_COLOR = 1,
    MOTOR_ANALYSIS_BAR_BORDER_COLOR = 10,
    MOTOR_ANALYSIS_BAR_FILL_COLOR = 6,
    MOTOR_ANALYSIS_CHART_LEFT = 3,
    MOTOR_ANALYSIS_CHART_RIGHT = 122,
    MOTOR_ANALYSIS_CHART_TOP = 23,
    MOTOR_ANALYSIS_CHART_BOTTOM = 62,
    MOTOR_ANALYSIS_SEPARATOR_X = 150,
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
 * @brief Draws one vertical diagnostic line.
 *
 * Fills every drawable pixel between the inclusive endpoints.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Horizontal coordinate.
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
 * @brief Draws one horizontal diagnostic line.
 *
 * Fills every drawable pixel between the inclusive endpoints.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] first_x First horizontal coordinate.
 * @param[in] last_x Last horizontal coordinate.
 * @param[in] y Vertical coordinate.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_horizontal(DisplayFramebuffer framebuffer, uint16_t first_x, uint16_t last_x,
                            uint16_t y, uint8_t color) {
    for (uint16_t x = first_x; x <= last_x; x++) {
        display_framebuffer_set_pixel(framebuffer, x, y, color);
    }
}

/**
 * @brief Draws the signed five-second torque chart.
 *
 * Renders five one-second divisions, six torque divisions, and retained samples in chronological
 * order with zero torque at the chart midpoint.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Retained scaled torque samples and ring position.
 */
static void draw_chart(DisplayFramebuffer framebuffer, const DisplayMotorDataAnalysisPage *page) {
    for (uint16_t x = MOTOR_ANALYSIS_CHART_LEFT; x <= MOTOR_ANALYSIS_CHART_RIGHT; x += 24) {
        draw_vertical(framebuffer, x, MOTOR_ANALYSIS_CHART_TOP, MOTOR_ANALYSIS_CHART_BOTTOM,
                      MOTOR_ANALYSIS_GRID_COLOR);
    }
    for (uint8_t division = 0; division <= 6; division++) {
        uint16_t y = (uint16_t)(MOTOR_ANALYSIS_CHART_TOP + division * 39u / 6u);
        draw_horizontal(framebuffer, MOTOR_ANALYSIS_CHART_LEFT, MOTOR_ANALYSIS_CHART_RIGHT, y,
                        MOTOR_ANALYSIS_GRID_COLOR);
    }
    draw_vertical(framebuffer, MOTOR_ANALYSIS_CHART_LEFT, MOTOR_ANALYSIS_CHART_TOP,
                  MOTOR_ANALYSIS_CHART_BOTTOM, MOTOR_ANALYSIS_COLOR);
    draw_horizontal(framebuffer, MOTOR_ANALYSIS_CHART_LEFT, MOTOR_ANALYSIS_CHART_RIGHT,
                    MOTOR_ANALYSIS_CHART_BOTTOM, MOTOR_ANALYSIS_COLOR);

    uint16_t oldest =
        page->sample_count < DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT ? 0 : page->next_sample;
    uint16_t first_x = (uint16_t)(MOTOR_ANALYSIS_CHART_RIGHT - page->sample_count + 1u);
    uint16_t previous_y = MOTOR_ANALYSIS_CHART_BOTTOM;
    for (uint16_t offset = 0; offset < page->sample_count; offset++) {
        uint16_t index = (uint16_t)((oldest + offset) % DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT);
        uint16_t y = (uint16_t)(MOTOR_ANALYSIS_CHART_BOTTOM - page->samples[index]);
        uint16_t first_y = offset == 0 || y < previous_y ? y : previous_y;
        uint16_t last_y = offset == 0 || y > previous_y ? y : previous_y;
        draw_vertical(framebuffer, (uint16_t)(first_x + offset), first_y, last_y,
                      MOTOR_ANALYSIS_SERIES_COLOR);
        previous_y = y;
    }
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
    draw_vertical(framebuffer, 137, 23, 62, MOTOR_ANALYSIS_BAR_BORDER_COLOR);
    draw_vertical(framebuffer, 143, 23, 62, MOTOR_ANALYSIS_BAR_BORDER_COLOR);
    draw_horizontal(framebuffer, 137, 143, 23, MOTOR_ANALYSIS_BAR_BORDER_COLOR);
    draw_horizontal(framebuffer, 137, 143, 42, MOTOR_ANALYSIS_BAR_BORDER_COLOR);
    draw_horizontal(framebuffer, 137, 143, 62, MOTOR_ANALYSIS_BAR_BORDER_COLOR);
    uint16_t fill = (uint16_t)(18u * percentage / 100u);
    if (fill == 0) {
        return;
    }
    for (uint16_t x = 138; x < 143; x++) {
        if (page->torque >= 0) {
            draw_vertical(framebuffer, x, (uint16_t)(41u - fill), 41,
                          MOTOR_ANALYSIS_BAR_FILL_COLOR);
        } else {
            draw_vertical(framebuffer, x, 43, (uint16_t)(43u + fill),
                          MOTOR_ANALYSIS_BAR_FILL_COLOR);
        }
    }
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
 * 30 milliseconds with a ten-second hold, and retains changed temperatures and primary fan speed.
 *
 * @param[in,out] page Retained motor-analysis state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] torque Signed torque in thousandths of a newton-metre.
 * @param[in] motor_temperature Motor temperature in degrees Celsius.
 * @param[in] driver_temperature Driver temperature in degrees Celsius.
 * @param[in] fan_speed_rpm Primary fan speed in revolutions per minute.
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
 * Clears the previous page and centers the title presented for the first second.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
void display_motor_data_analysis_page_render_title(DisplayFramebuffer framebuffer) {
    display_framebuffer_clear(framebuffer);
    display_text_draw_centered(framebuffer, "Motor Data Analysis Screen", 28, 1,
                               MOTOR_ANALYSIS_COLOR);
}

/**
 * @brief Renders live motor torque and supporting telemetry.
 *
 * Shows current and ten-second peak torque, a signed five-second chart, directional level bars,
 * motor and driver temperatures, and primary fan speed.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] page Current motor-analysis state.
 */
void display_motor_data_analysis_page_render(DisplayFramebuffer framebuffer,
                                             const DisplayMotorDataAnalysisPage *page) {
    char value[16];
    display_framebuffer_clear(framebuffer);
    display_text_draw(framebuffer, "M:", 1, 2, 1, MOTOR_ANALYSIS_COLOR);
    format_torque(value, page->torque);
    display_text_draw(framebuffer, value, 16, 2, 1, MOTOR_ANALYSIS_COLOR);
    display_text_draw(framebuffer, "Mpk(10s):", 1, 12, 1, MOTOR_ANALYSIS_COLOR);
    format_torque(value, page->peak_torque);
    display_text_draw(framebuffer, value, 61, 12, 1, MOTOR_ANALYSIS_COLOR);
    draw_chart(framebuffer, page);
    draw_torque_bars(framebuffer, page);
    draw_vertical(framebuffer, MOTOR_ANALYSIS_SEPARATOR_X, 0, 62, MOTOR_ANALYSIS_COLOR);

    display_text_draw(framebuffer, "Motor Data", 155, 1, 1, MOTOR_ANALYSIS_COLOR);
    display_text_draw(framebuffer, "Fan Speed:", 155, 12, 1, MOTOR_ANALYSIS_COLOR);
    format_fan_speed(value, page->fan_speed_rpm);
    display_text_draw(framebuffer, value, 155, 20, 1, MOTOR_ANALYSIS_COLOR);
    display_text_draw(framebuffer, "Tmp Motor:", 155, 30, 1, MOTOR_ANALYSIS_COLOR);
    format_temperature(value, page->motor_temperature);
    display_text_draw(framebuffer, value, 155, 38, 1, MOTOR_ANALYSIS_COLOR);
    display_text_draw(framebuffer, "Tmp Driver:", 155, 47, 1, MOTOR_ANALYSIS_COLOR);
    format_temperature(value, page->driver_temperature);
    display_text_draw(framebuffer, value, 155, 55, 1, MOTOR_ANALYSIS_COLOR);
}
