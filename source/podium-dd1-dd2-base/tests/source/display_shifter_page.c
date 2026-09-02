#include <assert.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/shifter_page.h"

static uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];

static uint8_t pixel(uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[(uint32_t)y * DISPLAY_FRAMEBUFFER_WIDTH / 2u + x / 2u];
    return (x & 1u) == 0 ? packed >> 4 : packed & 0x0fu;
}

static void test_gear_uses_only_middle_position(void) {
    ShifterLocalDisplay presentation = {
        .kind = SHIFTER_LOCAL_DISPLAY_GEAR,
        .glyph = 0x5b,
    };
    display_shifter_page_render(framebuffer, &presentation);
    assert(pixel(123, 18) == 15);
    assert(pixel(98, 18) == 0);
}

static void test_calibration_prompt_records(void) {
    ShifterLocalDisplay presentation = {
        .kind = SHIFTER_LOCAL_DISPLAY_CALIBRATION,
        .calibration_prompt = H_PATTERN_CALIBRATION_PROMPT_WAITING,
    };
    display_shifter_page_render(framebuffer, &presentation);
    assert(pixel(111, 21) == 15);
    presentation.calibration_prompt = H_PATTERN_CALIBRATION_PROMPT_POSITION;
    display_shifter_page_render(framebuffer, &presentation);
    assert(pixel(1, 16) == 8);
    assert(pixel(1, 56) == 8);
}

int main(void) {
    test_gear_uses_only_middle_position();
    test_calibration_prompt_records();
    return 0;
}
