#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "display/framebuffer.h"
#include "display/prompt.h"

static void test_render_and_clear(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint16_t lit = 0;

    display_prompt_render(framebuffer, true);
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        if (framebuffer[index] != 0) {
            lit++;
        }
    }
    assert(lit > 100);

    display_prompt_render(framebuffer, false);
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        assert(framebuffer[index] == 0);
    }
}

static void test_acknowledge_on_release(void) {
    DisplayPrompt prompt = {0};

    assert(!display_prompt_update(&prompt, true, false));
    assert(!display_prompt_update(&prompt, true, true));
    assert(prompt.input_seen);
    assert(!display_prompt_update(&prompt, true, true));
    assert(display_prompt_update(&prompt, true, false));
    assert(!prompt.input_seen);
    assert(!display_prompt_update(&prompt, true, false));
}

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * (DISPLAY_FRAMEBUFFER_WIDTH / 2) + x / 2];
    return (x & 1u) == 0 ? packed >> 4 : packed & 0x0fu;
}

static bool has_dark_pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x,
                           uint16_t y, uint16_t width, uint16_t height) {
    for (uint16_t row = y; row < y + height; row++) {
        for (uint16_t column = x; column < x + width; column++) {
            if (pixel(framebuffer, column, row) < 15) {
                return true;
            }
        }
    }
    return false;
}

static void assert_filled_overlay(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE]) {
    assert(pixel(framebuffer, 2, 16) == 15);
    assert(pixel(framebuffer, 252, 62) == 15);
    assert(pixel(framebuffer, 253, 62) == 0);
    assert(pixel(framebuffer, 1, 16) == 0);
    assert(pixel(framebuffer, 254, 16) == 0);
    assert(pixel(framebuffer, 2, 15) == 0);
}

static void test_render_enable_torque_prompt(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_prompt_render(framebuffer, true);

    assert_filled_overlay(framebuffer);
    assert(pixel(framebuffer, 127, 16) == 12);
    assert(pixel(framebuffer, 128, 16) == 0);
    assert(pixel(framebuffer, 129, 16) == 12);
    assert(has_dark_pixel(framebuffer, 2, 37, 251, 10));
}

static void test_render_torque_key_prompt(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_prompt_render_torque_key(framebuffer, true);
    assert_filled_overlay(framebuffer);
    assert(pixel(framebuffer, 127, 16) == 12);
    assert(pixel(framebuffer, 128, 16) == 0);
    assert(pixel(framebuffer, 129, 16) == 12);

    bool primary_text_present = false;
    bool secondary_text_present = false;
    bool acknowledgement_present = false;
    for (uint16_t y = 30; y < 40; y++) {
        for (uint16_t x = 2; x < 253; x++) {
            primary_text_present |= pixel(framebuffer, x, y) < 15;
        }
    }
    for (uint16_t y = 40; y < 50; y++) {
        for (uint16_t x = 2; x < 253; x++) {
            secondary_text_present |= pixel(framebuffer, x, y) < 15;
        }
    }
    acknowledgement_present = pixel(framebuffer, 120, 52) == 0;
    assert(primary_text_present);
    assert(secondary_text_present);
    assert(acknowledgement_present);
}

static void test_render_bite_point(void) {
    uint8_t first[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t second[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    uint8_t third[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    display_prompt_render_bite_point(first, true, 0);
    display_prompt_render_bite_point(second, true, 73);
    display_prompt_render_bite_point(third, true, 100);
    assert(memcmp(first, second, sizeof(first)) != 0);
    assert(memcmp(second, third, sizeof(second)) != 0);

    display_prompt_render_bite_point(third, false, 100);
    for (uint16_t index = 0; index < DISPLAY_FRAMEBUFFER_SIZE; index++) {
        assert(third[index] == 0);
    }
}

static void test_hide_clears_input(void) {
    DisplayPrompt prompt = {0};

    assert(!display_prompt_update(&prompt, true, true));
    assert(!display_prompt_update(&prompt, false, false));
    assert(!prompt.input_seen);
    assert(!display_prompt_update(&prompt, true, false));
}

int main(void) {
    test_render_and_clear();
    test_render_enable_torque_prompt();
    test_render_torque_key_prompt();
    test_render_bite_point();
    test_acknowledge_on_release();
    test_hide_clears_input();
    return 0;
}
