#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"
#include "display/temperature_analysis_page.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };
enum { TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS = 2250 };

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

static void assert_scale_label(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE],
                               const char *text, uint16_t x, uint16_t y) {
    uint8_t expected[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    display_text_draw(expected, text, x, y, 1, 15);
    for (uint16_t row = y; row < y + 10; row++) {
        for (uint16_t column = x; column < 25; column++) {
            assert(pixel(framebuffer, column, row) == pixel(expected, column, row));
        }
    }
}

static void samples_all_four_charts_once_per_global_tick(void) {
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {-10, 60, 120, 200};

    assert(display_temperature_analysis_page_update(&page, 0, temperatures, 0, 0));
    assert(page.next_sample == 0);
    assert(!display_temperature_analysis_page_update(
        &page, TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS - 1u, temperatures, 0, 0));
    assert(page.next_sample == 0);
    assert(display_temperature_analysis_page_update(
        &page, TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS, temperatures, 0, 0));
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR][0] == 0);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_DRIVER][0] == 14);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_BASE][0] == 29);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_QUICK_RELEASE][0] == 29);
    assert(page.next_sample == 1);
    assert(!display_temperature_analysis_page_update(
        &page, TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS, temperatures, 0, 0));
    assert(page.next_sample == 1);
    assert(!display_temperature_analysis_page_update(
        &page, TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS + 1u, temperatures, 0, 0));
    assert(page.next_sample == 1);
    const int16_t delayed_temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {0};
    assert(display_temperature_analysis_page_update(&page, 5000, delayed_temperatures, 0, 0));
    assert(page.next_sample == 1);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR][1] == 0);
    assert(display_temperature_analysis_page_update(&page, 6750, delayed_temperatures, 0, 0));
    assert(page.next_sample == 2);
}

static void retains_histories_and_updates_live_cooling_values(void) {
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {30, 40, 50, 60};

    assert(display_temperature_analysis_page_update(&page, 1, temperatures, 1234, 75));
    assert(page.next_sample == 0);
    assert(page.fan_speed_rpm == 1234);
    assert(page.power_percent == 75);
    assert(!display_temperature_analysis_page_update(&page, 1000, temperatures, 1234, 75));
    assert(display_temperature_analysis_page_update(
        &page, TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS, temperatures, 1234, 75));
    assert(page.next_sample == 1);
    assert(display_temperature_analysis_page_update(&page, 4500, temperatures, 1234, 75));
    assert(page.next_sample == 2);
}

static void preserves_transient_ring_index_at_capacity(void) {
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {0};
    const int16_t overwrite_temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {120, 120,
                                                                                         120, 120};

    assert(!display_temperature_analysis_page_update(&page, 0, temperatures, 0, 0));
    for (uint8_t sample = 1; sample <= DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT; sample++) {
        assert(display_temperature_analysis_page_update(
            &page, (uint32_t)sample * TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS, temperatures, 0,
            0));
    }
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR][0] == 0);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR][39] == 0);
    assert(page.next_sample == DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT);

    assert(display_temperature_analysis_page_update(
        &page, 41u * TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS, overwrite_temperatures, 0, 0));
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR][0] == 29);
    assert(page.next_sample == 1);
}

static void renders_from_transient_ring_index(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayTemperatureAnalysisPage page = {0};
    page.next_sample = DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT;
    page.samples[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR][1] = 29;

    display_temperature_analysis_page_render(framebuffer, &page);

    assert(pixel(framebuffer, 26, 28) == 4);
    assert(pixel(framebuffer, 27, 57) == 4);
}

static void renders_title_charts_fan_and_power(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {120, 80, 40, 0};

    display_temperature_analysis_page_render_title(framebuffer);
    assert(has_lit_pixel(framebuffer, 0, DISPLAY_FRAMEBUFFER_WIDTH - 1, 12, 18));
    assert(pixel(framebuffer, 0, 12) != 0);
    assert(pixel(framebuffer, 0, 22) == 0);

    assert(display_temperature_analysis_page_update(
        &page, TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS, temperatures, 1234, 75));
    display_temperature_analysis_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 25, 58) == 15);
    assert(pixel(framebuffer, 65, 58) == 0);
    assert(pixel(framebuffer, 199, 13) == 15);
    assert(pixel(framebuffer, 199, 40) == 15);
    assert(pixel(framebuffer, 199, 41) == 0);
    assert(pixel(framebuffer, 199, 60) == 15);
    assert(pixel(framebuffer, 199, 61) == 0);
    assert(pixel(framebuffer, 26, 57) == 4);
    assert(pixel(framebuffer, 64, 28) == 0);
    assert_scale_label(framebuffer, "120.0", 0, 23);
    assert_scale_label(framebuffer, "80.0", 5, 33);
    assert_scale_label(framebuffer, "40.0", 5, 43);
    assert_scale_label(framebuffer, "0.0", 10, 53);
    assert(has_lit_pixel(framebuffer, 2, 190, 13, 21));
    assert(has_lit_pixel(framebuffer, 200, 250, 13, 60));
}

static void renders_rising_temperature_line(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t cold_temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {0};
    const int16_t hot_temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {120, 0, 0, 0};

    assert(!display_temperature_analysis_page_update(&page, 0, cold_temperatures, 0, 0));
    assert(display_temperature_analysis_page_update(
        &page, TEMPERATURE_ANALYSIS_SAMPLE_INTERVAL_MS, cold_temperatures, 0, 0));
    assert(display_temperature_analysis_page_update(&page, 4500, hot_temperatures, 0, 0));
    display_temperature_analysis_page_render(framebuffer, &page);

    assert(pixel(framebuffer, 63, 57) == 4);
    assert(pixel(framebuffer, 64, 28) == 4);
}

int main(void) {
    samples_all_four_charts_once_per_global_tick();
    retains_histories_and_updates_live_cooling_values();
    preserves_transient_ring_index_at_capacity();
    renders_from_transient_ring_index();
    renders_title_charts_fan_and_power();
    renders_rising_temperature_line();
    return 0;
}
