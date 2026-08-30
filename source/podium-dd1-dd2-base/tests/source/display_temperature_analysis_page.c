#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/temperature_analysis_page.h"

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

static void samples_all_four_charts_every_two_and_a_quarter_seconds(void) {
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {-10, 60, 120, 200};
    display_temperature_analysis_page_open(&page, 1000);

    assert(!display_temperature_analysis_page_update(&page, 2249, temperatures, 0, 0));
    assert(display_temperature_analysis_page_update(&page, 2250, temperatures, 0, 0));
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR][0] == 0);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_DRIVER][0] == 14);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_BASE][0] == 29);
    assert(page.samples[DISPLAY_TEMPERATURE_ANALYSIS_QUICK_RELEASE][0] == 29);
    assert(page.sample_count == 1);
    assert(page.next_sample == 1);
}

static void retains_histories_and_updates_live_cooling_values(void) {
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {30, 40, 50, 60};
    display_temperature_analysis_page_open(&page, 0);

    assert(display_temperature_analysis_page_update(&page, 1, temperatures, 1234, 75));
    assert(page.sample_count == 0);
    assert(page.fan_speed_rpm == 1234);
    assert(page.power_percent == 75);
    assert(display_temperature_analysis_page_update(&page, 2250, temperatures, 1234, 75));
    display_temperature_analysis_page_open(&page, 5000);
    assert(page.sample_count == 1);
    assert(page.next_sample == 1);
}

static void renders_title_charts_fan_and_power(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayTemperatureAnalysisPage page = {0};
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT] = {120, 80, 40, 0};
    display_temperature_analysis_page_open(&page, 0);

    display_temperature_analysis_page_render_title(framebuffer);
    assert(has_lit_pixel(framebuffer, 0, DISPLAY_FRAMEBUFFER_WIDTH - 1, 28, 34));

    assert(display_temperature_analysis_page_update(&page, 2250, temperatures, 1234, 75));
    display_temperature_analysis_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 25, 58) == 15);
    assert(pixel(framebuffer, 199, 13) == 15);
    assert(pixel(framebuffer, 199, 61) == 15);
    assert(pixel(framebuffer, 64, 28) == 8);
    assert(has_lit_pixel(framebuffer, 2, 190, 13, 21));
    assert(has_lit_pixel(framebuffer, 200, 250, 13, 60));
}

int main(void) {
    samples_all_four_charts_every_two_and_a_quarter_seconds();
    retains_histories_and_updates_live_cooling_values();
    renders_title_charts_fan_and_power();
    return 0;
}
