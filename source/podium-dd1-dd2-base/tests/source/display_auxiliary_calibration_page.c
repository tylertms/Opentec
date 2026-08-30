#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "display/auxiliary_calibration_page.h"
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

static void retains_raw_glyphs_decimal_points_and_itm_state(void) {
    DisplayAuxiliaryCalibrationPage page = {0};
    const uint8_t glyphs[3] = {0x39, 0xf7, 0x38};

    assert(display_auxiliary_calibration_page_update(&page, glyphs, true));
    assert(page.glyphs[0] == 0x39);
    assert(page.glyphs[1] == 0xf7);
    assert(page.glyphs[2] == 0x38);
    assert(page.remote_tuning_active);
    assert(!display_auxiliary_calibration_page_update(&page, glyphs, true));
}

static void renders_title_segments_decimal_point_and_itm_label(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplayAuxiliaryCalibrationPage page = {0};
    const uint8_t glyphs[3] = {0x39, 0xf7, 0x38};
    display_auxiliary_calibration_page_update(&page, glyphs, true);

    display_auxiliary_calibration_page_render_title(framebuffer);
    assert(has_lit_pixel(framebuffer, 0, DISPLAY_FRAMEBUFFER_WIDTH - 1, 28, 34));

    display_auxiliary_calibration_page_render(framebuffer, &page);
    assert(pixel(framebuffer, 98, 18) == 15);
    assert(pixel(framebuffer, 95, 21) == 15);
    assert(pixel(framebuffer, 142, 55) == 15);
    assert(pixel(framebuffer, 148, 55) == 15);
    assert(has_lit_pixel(framebuffer, 235, 252, 50, 56));

    assert(display_auxiliary_calibration_page_update(&page, glyphs, false));
    display_auxiliary_calibration_page_render(framebuffer, &page);
    assert(!has_lit_pixel(framebuffer, 235, 252, 50, 56));
}

int main(void) {
    retains_raw_glyphs_decimal_points_and_itm_state();
    renders_title_segments_decimal_point_and_itm_label();
    return 0;
}
