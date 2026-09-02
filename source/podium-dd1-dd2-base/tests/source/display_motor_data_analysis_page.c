#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/motor_data_analysis_page.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) != 0 ? packed & 0x0fu : packed >> 4;
}

static bool has_lit_pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t first_x,
                          uint16_t last_x, uint16_t first_y, uint16_t last_y) {
    for (uint16_t y = first_y; y <= last_y; y++) {
        for (uint16_t x = first_x; x <= last_x; x++) {
            if (pixel(framebuffer, x, y) != 0) {
                return true;
            }
        }
    }
    return false;
}

static void samples_dd1_torque_on_the_global_chart_cadence(void) {
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 1000, BOARD_VARIANT_DD1);

    assert(page.torque_limit == 20000);
    assert(!display_motor_data_analysis_page_update(&page, 1024, 0, 0, 0, 0));
    assert(display_motor_data_analysis_page_update(&page, 1025, 20000, 0, 0, 0));
    assert(page.torque == 20000);
    assert(page.samples[0] == 39);
    assert(page.sample_count == 1);
    assert(page.next_sample == 1);
}

static void holds_absolute_peak_torque_for_ten_seconds(void) {
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD2);

    assert(display_motor_data_analysis_page_update(&page, 0, -12000, 0, 0, 0));
    assert(page.peak_torque == -12000);
    assert(page.peak_magnitude == 12000);
    assert(page.peak_expiry_ms == 10000);
    assert(!display_motor_data_analysis_page_update(&page, 30, 8000, 0, 0, 0));
    assert(page.peak_torque == -12000);
    assert(display_motor_data_analysis_page_update(&page, 10000, 8000, 0, 0, 0));
    assert(page.peak_torque == 8000);
    assert(page.peak_magnitude == 8000);
}

static void retains_history_and_peak_when_reopened(void) {
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD1);
    assert(display_motor_data_analysis_page_update(&page, 41, 15000, 20, 30, 1200));

    display_motor_data_analysis_page_open(&page, 1000, BOARD_VARIANT_DD2);
    assert(page.sample_count == 1);
    assert(page.next_sample == 1);
    assert(page.peak_torque == 15000);
    assert(page.torque_limit == 25000);
}

static void renders_title_chart_telemetry_and_directional_bars(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD1);

    display_motor_data_analysis_page_render_title(framebuffer);
    assert(has_lit_pixel(framebuffer, 0, DISPLAY_FRAMEBUFFER_WIDTH - 1, 12, 18));
    assert(pixel(framebuffer, 0, 12) != 0);
    assert(pixel(framebuffer, 0, 22) == 0);

    assert(display_motor_data_analysis_page_update(&page, 41, 20000, 42, -5, 1234));
    display_motor_data_analysis_page_render(framebuffer, &page);
    assert(has_lit_pixel(framebuffer, 1, 120, 2, 18));
    assert(pixel(framebuffer, 3, 63) == 15);
    assert(pixel(framebuffer, 123, 63) == 0);
    assert(pixel(framebuffer, 150, 62) == 15);
    assert(pixel(framebuffer, 150, 63) == 0);
    assert(has_lit_pixel(framebuffer, 153, 250, 13, 62));
    assert(pixel(framebuffer, 140, 24) == 10);
    assert(pixel(framebuffer, 140, 25) == 6);
    assert(pixel(framebuffer, 140, 42) == 6);
    assert(pixel(framebuffer, 140, 43) == 10);

    assert(display_motor_data_analysis_page_update(&page, 82, -20000, 42, -5, 1234));
    display_motor_data_analysis_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 140, 24) == 10);
    assert(pixel(framebuffer, 140, 25) == 0);
    assert(pixel(framebuffer, 140, 43) == 10);
    assert(pixel(framebuffer, 140, 44) == 6);
    assert(pixel(framebuffer, 140, 60) == 6);
    assert(pixel(framebuffer, 140, 61) == 2);
    assert(pixel(framebuffer, 140, 62) == 10);

    assert(display_motor_data_analysis_page_update(&page, 123, 0, 42, -5, 1234));
    display_motor_data_analysis_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 140, 43) == 10);
    assert(pixel(framebuffer, 140, 44) == 0);
}

int main(void) {
    samples_dd1_torque_on_the_global_chart_cadence();
    holds_absolute_peak_torque_for_ten_seconds();
    retains_history_and_peak_when_reopened();
    renders_title_chart_telemetry_and_directional_bars();
    return 0;
}
