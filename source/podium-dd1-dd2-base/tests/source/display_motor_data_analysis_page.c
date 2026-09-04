#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "display/framebuffer.h"
#include "display/motor_data_analysis_page.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) != 0 ? packed & 0x0fu : packed >> 4;
}

static bool framebuffer_region_equal(const uint8_t first[DISPLAY_FRAMEBUFFER_SIZE],
                                     const uint8_t second[DISPLAY_FRAMEBUFFER_SIZE],
                                     uint16_t first_x, uint16_t last_x, uint16_t first_y,
                                     uint16_t last_y) {
    for (uint16_t y = first_y; y <= last_y; y++) {
        for (uint16_t x = first_x; x <= last_x; x++) {
            if (pixel(first, x, y) != pixel(second, x, y)) {
                return false;
            }
        }
    }
    return true;
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

    assert(!display_motor_data_analysis_page_update(&page, 0, -12000, 0, 0, 0));
    assert(display_motor_data_analysis_page_update(&page, 1, -12000, 0, 0, 0));
    assert(page.peak_torque == -12000);
    assert(page.peak_magnitude == 12000);
    assert(page.peak_expiry_ms == 10001);
    assert(!display_motor_data_analysis_page_update(&page, 30, 8000, 0, 0, 0));
    assert(page.peak_torque == -12000);
    assert(display_motor_data_analysis_page_update(&page, 10000, 8000, 0, 0, 0));
    assert(display_motor_data_analysis_page_update(&page, 10001, 8000, 0, 0, 0));
    assert(!display_motor_data_analysis_page_update(&page, 10030, 8000, 0, 0, 0));
    assert(display_motor_data_analysis_page_update(&page, 10031, 8000, 0, 0, 0));
    assert(page.peak_torque == 8000);
    assert(page.peak_magnitude == 8000);
}

static void applies_strict_after_peak_deadlines(void) {
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD2);
    page.next_sample_ms = UINT32_MAX / 2u;
    page.next_peak_update_ms = 100;
    page.peak_expiry_ms = 131;
    page.peak_torque = 1000;
    page.peak_magnitude = 1000;

    assert(!display_motor_data_analysis_page_update(&page, 100, 0, 0, 0, 0));
    assert(!display_motor_data_analysis_page_update(&page, 101, 0, 0, 0, 0));
    assert(!display_motor_data_analysis_page_update(&page, 131, 0, 0, 0, 0));
    assert(display_motor_data_analysis_page_update(&page, 132, 0, 0, 0, 0));
    assert(page.peak_torque == 0);

    page.next_peak_update_ms = 0;
    page.peak_expiry_ms = 0;
    page.peak_torque = 1000;
    page.peak_magnitude = 1000;
    assert(display_motor_data_analysis_page_update(&page, UINT32_MAX, 0, 0, 0, 0));
    assert(page.peak_torque == 0);
}

static void narrows_torque_percentage_before_direction(void) {
    static const struct {
        BoardVariant variant;
        int16_t torque;
        uint8_t positive_percentage;
        uint8_t negative_percentage;
    } cases[] = {
        {BOARD_VARIANT_DD1, 32767, 0, 93},
        {BOARD_VARIANT_DD1, -32768, 93, 0},
        {BOARD_VARIANT_DD2, 32000, 0, 128},
        {BOARD_VARIANT_DD2, 32767, 0, 99},
        {BOARD_VARIANT_DD2, -32768, 99, 0},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        DisplayMotorDataAnalysisPage page = {0};
        display_motor_data_analysis_page_open(&page, 0, cases[index].variant);
        assert(display_motor_data_analysis_page_update(&page, 41, cases[index].torque, 0, 0, 0));
        assert(page.primary_percentage[0] == cases[index].positive_percentage);
        assert(page.primary_percentage[1] == cases[index].negative_percentage);
    }
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

static void holds_split_progress_peak_for_five_seconds_then_decays(void) {
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD1);

    assert(display_motor_data_analysis_page_update(&page, 41, 20000, 0, 0, 0));
    assert(page.primary_percentage[0] == 99);
    assert(page.secondary_percentage[0] == 0);
    assert(display_motor_data_analysis_page_update(&page, 42, 20000, 0, 0, 0));
    assert(page.secondary_percentage[0] == 99);
    assert(page.decay_countdown[0] == 5000);

    assert(display_motor_data_analysis_page_update(&page, 5042, 0, 0, 0, 0));
    assert(page.secondary_percentage[0] == 99);
    assert(display_motor_data_analysis_page_update(&page, 5043, 0, 0, 0, 0));
    assert(page.secondary_percentage[0] == 98);
}

static void preserves_transient_ring_index_for_chart_rotation(void) {
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD1);
    page.next_sample = DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT - 1u;
    page.next_sample_ms = 1;

    assert(display_motor_data_analysis_page_update(&page, 1, 0, 0, 0, 0));
    assert(page.next_sample == DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT);

    page.next_sample_ms = 2;
    assert(display_motor_data_analysis_page_update(&page, 2, 0, 0, 0, 0));
    assert(page.next_sample == 1);
}

static void renders_rising_chart_line_without_coordinate_wrap(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD1);
    page.samples[0] = 0;
    page.samples[1] = 39;
    page.next_sample = 119;

    display_motor_data_analysis_page_render(framebuffer, &page);

    assert(pixel(framebuffer, 4, 62) == 4);
    assert(pixel(framebuffer, 5, 23) == 4);
}

static void renders_title_chart_telemetry_and_directional_bars(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t positive_framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayMotorDataAnalysisPage page = {0};
    display_motor_data_analysis_page_open(&page, 0, BOARD_VARIANT_DD1);

    assert(display_motor_data_analysis_page_update(&page, 41, 20000, 42, -5, 1234));
    page.samples[0] = 0;
    page.samples[1] = 39;
    page.samples[2] = 0;
    page.next_sample = DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT;
    page.torque = 1600;
    page.peak_torque = 1900;
    display_motor_data_analysis_page_render(framebuffer, &page);
    memcpy(positive_framebuffer, framebuffer, sizeof(positive_framebuffer));
    assert(has_lit_pixel(framebuffer, 1, 120, 2, 18));
    assert(pixel(framebuffer, 3, 43) == 15);
    assert(pixel(framebuffer, 122, 43) == 15);
    assert(pixel(framebuffer, 123, 43) == 0);
    assert(pixel(framebuffer, 3, 63) == 0);
    assert(pixel(framebuffer, 4, 23) == 4);
    assert(pixel(framebuffer, 5, 62) == 4);
    assert(has_lit_pixel(framebuffer, 124, 135, 33, 42));
    assert(pixel(framebuffer, 150, 62) == 15);
    assert(pixel(framebuffer, 150, 63) == 0);
    assert(has_lit_pixel(framebuffer, 153, 250, 13, 62));
    assert(pixel(framebuffer, 140, 24) == 10);
    assert(pixel(framebuffer, 140, 25) == 6);
    assert(pixel(framebuffer, 140, 42) == 6);
    assert(pixel(framebuffer, 140, 43) == 10);

    display_motor_data_analysis_page_render_title(framebuffer);
    assert(has_lit_pixel(framebuffer, 0, DISPLAY_FRAMEBUFFER_WIDTH - 1, 12, 18));
    assert(pixel(framebuffer, 0, 12) != 0);
    assert(pixel(framebuffer, 3, 43) == pixel(positive_framebuffer, 3, 43));
    assert(pixel(framebuffer, 150, 62) == pixel(positive_framebuffer, 150, 62));

    assert(display_motor_data_analysis_page_update(&page, 82, -20000, 42, -5, 1234));
    page.torque = -1599;
    page.peak_torque = 1999;
    display_motor_data_analysis_page_render(framebuffer, &page);
    assert(framebuffer_region_equal(positive_framebuffer, framebuffer, 15, 62, 13, 22));
    assert(framebuffer_region_equal(positive_framebuffer, framebuffer, 108, 155, 13, 22));
    assert(pixel(framebuffer, 140, 24) == 10);
    assert(pixel(framebuffer, 140, 25) == 2);
    assert(pixel(framebuffer, 140, 42) == 2);
    assert(pixel(framebuffer, 140, 43) == 10);
    assert(pixel(framebuffer, 140, 44) == 6);
    assert(pixel(framebuffer, 140, 60) == 6);
    assert(pixel(framebuffer, 140, 61) == 0);
    assert(pixel(framebuffer, 140, 62) == 10);

    page.torque = 1551;
    page.peak_torque = 1600;
    display_motor_data_analysis_page_render(framebuffer, &page);
    assert(framebuffer_region_equal(positive_framebuffer, framebuffer, 15, 62, 13, 22));

    assert(display_motor_data_analysis_page_update(&page, 123, 0, 42, -5, 1234));
    assert(page.secondary_percentage[1] == 99);
}

int main(void) {
    samples_dd1_torque_on_the_global_chart_cadence();
    holds_absolute_peak_torque_for_ten_seconds();
    applies_strict_after_peak_deadlines();
    narrows_torque_percentage_before_direction();
    retains_history_and_peak_when_reopened();
    holds_split_progress_peak_for_five_seconds_then_decays();
    preserves_transient_ring_index_for_chart_rotation();
    renders_rising_chart_line_without_coordinate_wrap();
    renders_title_chart_telemetry_and_directional_bars();
    return 0;
}
