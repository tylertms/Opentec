#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/notice.h"

enum {
    DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2,
};

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) != 0 ? packed & 0x0fu : packed >> 4;
}

static bool has_lit_pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x,
                          uint16_t y, uint16_t width, uint16_t height) {
    for (uint16_t row = y; row < y + height; row++) {
        for (uint16_t column = x; column < x + width; column++) {
            if (pixel(framebuffer, column, row) != 0) {
                return true;
            }
        }
    }
    return false;
}

static void test_renders_persistent_notice_layout(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_notice_render_torque_disabled(framebuffer, true);

    assert(pixel(framebuffer, 128, 17) == 15);
    assert(pixel(framebuffer, 123, 26) == 15);
    assert(pixel(framebuffer, 133, 26) == 15);
    assert(pixel(framebuffer, 38, 37) == 15);
    assert(pixel(framebuffer, 216, 43) == 15);
}

static void test_hidden_notice_clears_display(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        framebuffer[index] = 0xff;
    }

    display_notice_render_torque_disabled(framebuffer, false);

    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        assert(framebuffer[index] == 0);
    }
}

static void test_renders_position_sensor_notices(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_notice_render_system(framebuffer, SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED);
    assert(has_lit_pixel(framebuffer, 123, 17, 11, 10));
    assert(has_lit_pixel(framebuffer, 0, 37, DISPLAY_FRAMEBUFFER_WIDTH, 7));

    display_notice_render_system(framebuffer, SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED);
    assert(pixel(framebuffer, 123, 17) == 15);
    assert(pixel(framebuffer, 133, 26) == 15);
    assert(has_lit_pixel(framebuffer, 0, 37, DISPLAY_FRAMEBUFFER_WIDTH, 7));
}

static void test_renders_two_line_torque_reduction_notice(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_notice_render_system(framebuffer, SYSTEM_NOTICE_TORQUE_REDUCED);

    assert(has_lit_pixel(framebuffer, 0, 30, DISPLAY_FRAMEBUFFER_WIDTH, 7));
    assert(has_lit_pixel(framebuffer, 0, 40, DISPLAY_FRAMEBUFFER_WIDTH, 7));
}

int main(void) {
    test_renders_persistent_notice_layout();
    test_hidden_notice_clears_display();
    test_renders_position_sensor_notices();
    test_renders_two_line_torque_reduction_notice();
    return 0;
}
