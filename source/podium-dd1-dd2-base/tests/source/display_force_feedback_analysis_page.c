#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "display/force_feedback_analysis_page.h"
#include "display/framebuffer.h"

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

static void advances_global_sampling_and_consumes_timer_updates(void) {
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 1000);
    assert(!display_force_feedback_analysis_page_tick(&page, 1000, 0, 0));

    assert(!display_force_feedback_analysis_page_tick(&page, 1024, 0, 0x8000));
    assert(display_force_feedback_analysis_page_tick(&page, 1025, 0, 0x8000));
    assert(page.render_pending);
    assert(display_force_feedback_analysis_page_update(&page));
    assert(!page.render_pending);
    assert(page.percentage == 50);
    assert(page.direction == 0);
    assert(display_force_feedback_analysis_page_tick(&page, 1026, 1, UINT16_MAX));
    assert(display_force_feedback_analysis_page_update(&page));
    assert(!display_force_feedback_analysis_page_tick(&page, 1049, 1, UINT16_MAX));
    assert(!display_force_feedback_analysis_page_update(&page));
    assert(display_force_feedback_analysis_page_tick(&page, 1050, 1, UINT16_MAX));
    assert(display_force_feedback_analysis_page_update(&page));
    assert(page.percentage == 99);
    assert(page.direction == 1);
}

static void keeps_timer_state_out_of_foreground_updates(void) {
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 1000);
    assert(page.open_pending);

    uint32_t next_sample_before_timer = page.next_sample_ms;
    uint32_t last_tick_before_timer = page.last_tick_ms;
    assert(!display_force_feedback_analysis_page_update(&page));
    assert(page.open_pending);
    assert(page.next_sample_ms == next_sample_before_timer);
    assert(page.last_tick_ms == last_tick_before_timer);

    assert(!display_force_feedback_analysis_page_tick(&page, 1000, 0, 0));
    assert(!page.open_pending);
    assert(page.next_sample_ms == 1025);
    assert(page.last_tick_ms == 1000);
    assert(display_force_feedback_analysis_page_tick(&page, 1025, 0, 0x8000));

    uint16_t next_sample_after_tick = page.next_sample_ms;
    uint32_t last_tick_after_tick = page.last_tick_ms;
    uint8_t sample_after_tick = page.samples[0];
    assert(display_force_feedback_analysis_page_update(&page));
    assert(page.next_sample_ms == next_sample_after_tick);
    assert(page.last_tick_ms == last_tick_after_tick);
    assert(page.samples[0] == sample_after_tick);
    assert(!page.render_pending);
    assert(!page.render_active);

    display_force_feedback_analysis_page_open(&page, 2000);
    assert(page.open_pending);
    page.render_active = true;
    assert(!display_force_feedback_analysis_page_tick(&page, 2001, 1, UINT16_MAX));
    assert(page.open_pending);
    assert(page.next_sample_ms == next_sample_after_tick);
    page.render_active = false;
    assert(!display_force_feedback_analysis_page_tick(&page, 2001, 1, UINT16_MAX));
    assert(!page.open_pending);
    assert(page.next_sample_ms == 2025);
    assert(page.last_tick_ms == 2001);
    assert(page.samples[0] == sample_after_tick);

    display_force_feedback_analysis_page_open(&page, 4000);
    display_force_feedback_analysis_page_open(&page, 5000);
    assert(page.open_pending);
    assert(!display_force_feedback_analysis_page_tick(&page, 5001, 0, 0));
    assert(!page.open_pending);
    assert(page.next_sample_ms == 5025);
    assert(page.last_tick_ms == 5001);
}

static void samples_at_twenty_five_millisecond_intervals(void) {
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 1000);
    assert(!display_force_feedback_analysis_page_tick(&page, 1000, 0, 0));

    assert(!display_force_feedback_analysis_page_update(&page));
    assert(!display_force_feedback_analysis_page_tick(&page, 1024, 0, 0x8000));
    assert(display_force_feedback_analysis_page_tick(&page, 1025, 0, 0x8000));
    assert(display_force_feedback_analysis_page_update(&page));
    assert(page.percentage == 50);
    assert(page.direction == 0);
    assert(display_force_feedback_analysis_page_tick(&page, 1049, 1, UINT16_MAX));
    assert(display_force_feedback_analysis_page_update(&page));
    assert(display_force_feedback_analysis_page_tick(&page, 1050, 1, UINT16_MAX));
    assert(display_force_feedback_analysis_page_update(&page));
    assert(page.percentage == 99);
    assert(page.direction == 1);
}

static void retains_peak_until_one_second_then_decays(void) {
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 0);
    assert(!display_force_feedback_analysis_page_tick(&page, 0, 0, 0));

    assert(display_force_feedback_analysis_page_tick(&page, 25, 0, UINT16_MAX));
    assert(page.primary_percentage[0] == 99);
    assert(page.secondary_percentage[0] == 0);
    assert(display_force_feedback_analysis_page_tick(&page, 26, 1, 0));
    assert(page.secondary_percentage[0] == 99);
    assert(page.decay_countdown[0] == 1000);
    assert(display_force_feedback_analysis_page_tick(&page, 50, 1, 0));
    assert(page.primary_percentage[0] == 0);
    assert(page.secondary_percentage[0] == 99);
    for (uint32_t now_ms = 51; now_ms <= 1026; now_ms++) {
        display_force_feedback_analysis_page_tick(&page, now_ms, 1, 0);
    }
    assert(page.secondary_percentage[0] == 99);
    assert(display_force_feedback_analysis_page_tick(&page, 1027, 1, 0));
    assert(page.secondary_percentage[0] == 98);
}

static void retains_the_latest_two_hundred_samples(void) {
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 0);
    assert(!display_force_feedback_analysis_page_tick(&page, 0, 0, 0));

    for (uint16_t index = 0; index <= DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT; index++) {
        assert(display_force_feedback_analysis_page_tick(
            &page, (uint32_t)(index + 1u) * 25u, 0, (uint16_t)(index * 327u)));
    }

    assert(page.next_sample == 1);
    assert(page.samples[0] == 38);
    display_force_feedback_analysis_page_open(&page, 6000);
    assert(!display_force_feedback_analysis_page_tick(&page, 6000, 0, 0));
    assert(page.next_sample == 1);
}

static void renders_the_title_and_analysis_content(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 0);
    assert(!display_force_feedback_analysis_page_tick(&page, 0, 0, 0));

    assert(display_force_feedback_analysis_page_tick(&page, 25, 0, 0x8000));
    assert(display_force_feedback_analysis_page_update(&page));
    display_force_feedback_analysis_page_render(framebuffer, &page);
    assert(has_lit_pixel(framebuffer, 4, 168, 12, 21));
    assert(pixel(framebuffer, 204, 62) == 15);
    assert(pixel(framebuffer, 205, 62) == 0);
    assert(pixel(framebuffer, 5, 62) == 15);
    assert(pixel(framebuffer, 5, 61) == 15);
    assert(pixel(framebuffer, 210, 22) == 10);
    assert(pixel(framebuffer, 215, 42) == 6);
    assert(pixel(framebuffer, 224, 42) == 6);
    assert(pixel(framebuffer, 225, 22) == 10);
    assert(pixel(framebuffer, 225, 61) == 10);
    assert(pixel(framebuffer, 230, 61) == 0);

    display_force_feedback_analysis_page_render_title(framebuffer);
    assert(has_lit_pixel(framebuffer, 0, DISPLAY_FRAMEBUFFER_WIDTH - 1, 12, 18));
    assert(pixel(framebuffer, 0, 12) != 0);
    assert(pixel(framebuffer, 215, 42) == 6);

    assert(display_force_feedback_analysis_page_tick(&page, 50, 1, 0x8000));
    assert(display_force_feedback_analysis_page_update(&page));
    display_force_feedback_analysis_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 215, 42) == 2);
    assert(pixel(framebuffer, 230, 42) == 6);
    display_force_feedback_analysis_page_render_title(framebuffer);
    assert(pixel(framebuffer, 0, 12) != 0);
    assert(pixel(framebuffer, 215, 42) == 2);
}

static void renders_the_secondary_endpoint_and_descending_series(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 0);
    assert(!display_force_feedback_analysis_page_tick(&page, 0, 0, 0));
    page.samples[0] = 0;
    page.samples[1] = 39;
    page.next_sample = 0;
    page.primary_percentage[0] = 0;
    page.secondary_percentage[0] = 99;

    display_force_feedback_analysis_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 6, 61) == 4);
    assert(pixel(framebuffer, 7, 22) == 4);
    assert(pixel(framebuffer, 224, 23) == 2);
    assert(pixel(framebuffer, 224, 61) == 2);
    assert(pixel(framebuffer, 225, 23) == 10);
}

int main(void) {
    advances_global_sampling_and_consumes_timer_updates();
    keeps_timer_state_out_of_foreground_updates();
    samples_at_twenty_five_millisecond_intervals();
    retains_peak_until_one_second_then_decays();
    retains_the_latest_two_hundred_samples();
    renders_the_title_and_analysis_content();
    renders_the_secondary_endpoint_and_descending_series();
    return 0;
}
