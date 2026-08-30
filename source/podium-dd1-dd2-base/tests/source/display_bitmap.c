#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "display/bitmap.h"
#include "display/framebuffer.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) != 0 ? packed & 0x0fu : packed >> 4;
}

static void draws_odd_width_rows_and_clamps_colors(void) {
    static const uint8_t bitmap[] = {0x12, 0xf0, 0x45, 0x60};
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_bitmap_draw(framebuffer, bitmap, 1, 1, 3, 2, false, 5);

    assert(pixel(framebuffer, 1, 1) == 1);
    assert(pixel(framebuffer, 2, 1) == 2);
    assert(pixel(framebuffer, 3, 1) == 5);
    assert(pixel(framebuffer, 1, 2) == 4);
    assert(pixel(framebuffer, 2, 2) == 5);
    assert(pixel(framebuffer, 3, 2) == 5);
}

static void inverts_clamped_colors(void) {
    static const uint8_t bitmap[] = {0x12, 0xf0, 0x45, 0x60};
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_bitmap_draw(framebuffer, bitmap, 1, 1, 3, 2, true, 5);

    assert(pixel(framebuffer, 1, 1) == 14);
    assert(pixel(framebuffer, 2, 1) == 13);
    assert(pixel(framebuffer, 3, 1) == 10);
    assert(pixel(framebuffer, 1, 2) == 11);
    assert(pixel(framebuffer, 2, 2) == 10);
    assert(pixel(framebuffer, 3, 2) == 10);
}

static void accepts_empty_dimensions(void) {
    static const uint8_t bitmap[] = {0xff};
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t expected[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_bitmap_draw(framebuffer, bitmap, 0, 0, 0, 1, false, 15);
    display_bitmap_draw(framebuffer, bitmap, 0, 0, 1, 0, false, 15);

    assert(memcmp(framebuffer, expected, sizeof(framebuffer)) == 0);
}

int main(void) {
    draws_odd_width_rows_and_clamps_colors();
    inverts_clamped_colors();
    accepts_empty_dimensions();
    return 0;
}
