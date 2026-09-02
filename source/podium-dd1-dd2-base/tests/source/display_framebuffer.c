#include <assert.h>
#include <stdint.h>

#include "display/framebuffer.h"

static void test_pixel_nibbles(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_framebuffer_set_pixel(framebuffer, 2, 1, 0x1a);
    assert(framebuffer[129] == 0xa0);
    display_framebuffer_set_pixel(framebuffer, 3, 1, 0x05);
    assert(framebuffer[129] == 0xa5);
    display_framebuffer_set_pixel(framebuffer, 2, 1, 0x03);
    assert(framebuffer[129] == 0x35);
}

static void test_bounds(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_framebuffer_set_pixel(framebuffer, 254, 62, 15);
    assert(framebuffer[62 * 128 + 127] == 0xf0);
    display_framebuffer_set_pixel(framebuffer, 255, 62, 15);
    display_framebuffer_set_pixel(framebuffer, 254, 63, 15);
    display_framebuffer_set_pixel(framebuffer, 255, 63, 10);
    assert(framebuffer[62 * 128 + 127] == 0xff);
    assert(framebuffer[63 * 128 + 127] == 0xfa);
}

static void test_clear(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        framebuffer[index] = 0xff;
    }

    display_framebuffer_clear(framebuffer);

    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        assert(framebuffer[index] == 0);
    }
}

int main(void) {
    test_pixel_nibbles();
    test_bounds();
    test_clear();
    return 0;
}
