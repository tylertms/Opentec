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

static void samples_at_twenty_five_millisecond_intervals(void) {
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 1000);

    assert(!display_force_feedback_analysis_page_update(&page, 1024, 0, 0x8000));
    assert(display_force_feedback_analysis_page_update(&page, 1025, 0, 0x8000));
    assert(page.percentage == 50);
    assert(page.direction == 0);
    assert(page.sample_count == 1);
    assert(!display_force_feedback_analysis_page_update(&page, 1049, 1, UINT16_MAX));
    assert(display_force_feedback_analysis_page_update(&page, 1050, 1, UINT16_MAX));
    assert(page.percentage == 99);
    assert(page.direction == 1);
    assert(page.sample_count == 2);
}

static void retains_the_latest_two_hundred_samples(void) {
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 0);

    for (uint16_t index = 0; index <= DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT; index++) {
        assert(display_force_feedback_analysis_page_update(&page, (uint32_t)(index + 1u) * 25u, 0,
                                                           (uint16_t)(index * 327u)));
    }

    assert(page.sample_count == DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT);
    assert(page.next_sample == 1);
    assert(page.samples[0] == 38);
    display_force_feedback_analysis_page_open(&page, 6000);
    assert(page.sample_count == DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT);
    assert(page.next_sample == 1);
}

static void renders_the_title_and_analysis_content(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayForceFeedbackAnalysisPage page = {0};
    display_force_feedback_analysis_page_open(&page, 0);

    display_force_feedback_analysis_page_render_title(framebuffer);
    assert(has_lit_pixel(framebuffer, 0, DISPLAY_FRAMEBUFFER_WIDTH - 1, 12, 18));
    assert(pixel(framebuffer, 0, 12) != 0);
    assert(pixel(framebuffer, 0, 22) == 0);

    assert(display_force_feedback_analysis_page_update(&page, 25, 0, 0x8000));
    display_force_feedback_analysis_page_render(framebuffer, &page);
    assert(has_lit_pixel(framebuffer, 4, 168, 12, 21));
    assert(pixel(framebuffer, 204, 62) == 15);
    assert(pixel(framebuffer, 205, 62) == 0);
    assert(pixel(framebuffer, 5, 62) == 15);
    assert(pixel(framebuffer, 5, 61) == 15);
    assert(pixel(framebuffer, 210, 22) == 10);
    assert(pixel(framebuffer, 215, 42) == 6);
    assert(pixel(framebuffer, 225, 22) == 10);
    assert(pixel(framebuffer, 225, 61) == 10);
    assert(pixel(framebuffer, 230, 61) == 0);

    assert(display_force_feedback_analysis_page_update(&page, 50, 1, 0x8000));
    display_force_feedback_analysis_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 215, 42) == 0);
    assert(pixel(framebuffer, 230, 42) == 6);
}

int main(void) {
    samples_at_twenty_five_millisecond_intervals();
    retains_the_latest_two_hundred_samples();
    renders_the_title_and_analysis_content();
    return 0;
}
