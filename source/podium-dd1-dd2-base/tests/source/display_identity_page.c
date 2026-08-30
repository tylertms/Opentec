#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "board/identity.h"
#include "display/framebuffer.h"
#include "display/identity_page.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };

static uint32_t hash_rows(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t first_y,
                          uint16_t last_y) {
    uint32_t hash = 2166136261u;
    for (uint16_t y = first_y; y <= last_y; y++) {
        for (uint16_t x = 0; x < DISPLAY_ROW_BYTES; x++) {
            hash ^= framebuffer[y * DISPLAY_ROW_BYTES + x];
            hash *= 16777619u;
        }
    }
    return hash;
}

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) != 0 ? packed & 0x0fu : packed >> 4;
}

static bool has_lit_pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t first_y,
                          uint16_t last_y) {
    for (uint16_t y = first_y; y <= last_y; y++) {
        for (uint16_t x = 0; x < DISPLAY_FRAMEBUFFER_WIDTH; x++) {
            if (pixel(framebuffer, x, y) != 0) {
                return true;
            }
        }
    }
    return false;
}

static void renders_the_root_identity_layout(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_identity_page_render(
        framebuffer, (BoardIdentity){.variant = BOARD_VARIANT_DD1, .hardware_option = 1});

    assert(!has_lit_pixel(framebuffer, 0, 18));
    assert(hash_rows(framebuffer, 19, 44) == 0x64d446b6u);
    assert(!has_lit_pixel(framebuffer, 45, 49));
    assert(has_lit_pixel(framebuffer, 50, 56));
}

static void distinguishes_dd1_and_dd2_model_text(void) {
    uint8_t dd1[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t dd2[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_identity_page_render(
        dd1, (BoardIdentity){.variant = BOARD_VARIANT_DD1, .hardware_option = 1});
    display_identity_page_render(
        dd2, (BoardIdentity){.variant = BOARD_VARIANT_DD2, .hardware_option = 1});

    assert(memcmp(dd1, dd2, sizeof(dd1)) != 0);
}

static void distinguishes_playstation_hardware_option(void) {
    uint8_t standard[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t playstation[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_identity_page_render(
        standard, (BoardIdentity){.variant = BOARD_VARIANT_DD1, .hardware_option = 0});
    display_identity_page_render(
        playstation, (BoardIdentity){.variant = BOARD_VARIANT_DD1, .hardware_option = 1});

    assert(memcmp(standard + 50 * DISPLAY_ROW_BYTES, playstation + 50 * DISPLAY_ROW_BYTES,
                  7 * DISPLAY_ROW_BYTES) != 0);
}

static void clears_previous_page_content(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    memset(framebuffer, 0xff, sizeof(framebuffer));

    display_identity_page_render(
        framebuffer, (BoardIdentity){.variant = BOARD_VARIANT_DD2, .hardware_option = 1});

    assert(pixel(framebuffer, 0, 0) == 0);
    assert(pixel(framebuffer, DISPLAY_FRAMEBUFFER_WIDTH - 1, DISPLAY_FRAMEBUFFER_HEIGHT - 1) == 0);
}

int main(void) {
    renders_the_root_identity_layout();
    distinguishes_dd1_and_dd2_model_text();
    distinguishes_playstation_hardware_option();
    clears_previous_page_content();
    return 0;
}
