#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "display/font.h"
#include "display/framebuffer.h"
#include "display/text.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) == 0 ? packed >> 4 : packed & 0x0fu;
}

static void assert_font_table(const DisplayFont *font, uint16_t height) {
    assert(font->count == 95);
    for (uint8_t index = 0; index < font->count; index++) {
        const DisplayFontCharacter *character = &font->characters[index];
        assert(character->character == (uint8_t)(0x20u + index));
        assert(character->glyph != NULL);
        assert(character->glyph->bitmap != NULL);
        assert(character->glyph->width != 0);
        assert(character->glyph->height == height);
        assert(character->glyph->format == 8);
    }
}

static void assert_glyph(const DisplayFont *font, uint8_t character, uint16_t width,
                         const uint8_t *bitmap, size_t bitmap_size) {
    const DisplayGlyph *glyph = font->characters[character - 0x20u].glyph;
    assert(glyph->width == width);
    assert(memcmp(glyph->bitmap, bitmap, bitmap_size) == 0);
}

static void test_font_tables_and_metrics(void) {
    static const uint8_t font10_exclamation[] = {
        0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x80, 0x00, 0x00,
    };
    static const uint8_t font10_percent[] = {
        0x00, 0x00, 0x62, 0x00, 0x94, 0x00, 0x94, 0x00, 0x6b, 0x00,
        0x14, 0x80, 0x14, 0x80, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    static const uint8_t font21_exclamation[] = {
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x20, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    assert_font_table(&display_font_10_00c988, 10);
    assert_font_table(&display_font_21_00aba6, 21);
    assert_glyph(&display_font_10_00c988, '!', 2, font10_exclamation, sizeof(font10_exclamation));
    assert_glyph(&display_font_10_00c988, '%', 10, font10_percent, sizeof(font10_percent));
    assert_glyph(&display_font_21_00aba6, '!', 6, font21_exclamation, sizeof(font21_exclamation));

    assert(display_text_width_for_font(&display_font_10_00c988, "ATTENTION Enable torque?") == 128);
    assert(display_text_width_for_font(&display_font_10_00c988,
                                       "CAUTION! Torque Key Inserted! Please read") == 199);
    assert(display_text_width_for_font(&display_font_10_00c988,
                                       "the manuals safety guidelines before use.") == 182);
    assert(display_text_width_for_font(&display_font_10_00c988,
                                       "Please disconnect steering wheel to calibrate motor.") ==
           230);
    assert(display_text_width_for_font(&display_font_10_00c988,
                                       "Motor cal. not supported by current firmware version.") ==
           244);
    assert(display_text_width(NULL, 1) == 0);
    assert(display_text_width("A", 0) == 0);
    assert(display_text_width("A", 1) == 7);
    assert(display_text_width("A", 2) == 13);
    assert(display_text_width("A", 3) == 0);
}

static void test_renderer_background_and_leading_column(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    memset(framebuffer, 0xff, sizeof(framebuffer));

    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "!", 0, 0, false);
    for (uint16_t y = 0; y < 10; y++) {
        assert(pixel(framebuffer, 0, y) == 0);
    }
    assert(pixel(framebuffer, 1, 0) == 0);
    assert(pixel(framebuffer, 1, 1) == 15);
    assert(pixel(framebuffer, 2, 1) == 0);
    assert(pixel(framebuffer, 3, 0) == 15);
}

static void test_renderer_inversion_and_variable_advance(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    memset(framebuffer, 0xff, sizeof(framebuffer));

    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "!", 10, 5, true);
    for (uint16_t y = 5; y < 15; y++) {
        assert(pixel(framebuffer, 10, y) == 15);
    }
    assert(pixel(framebuffer, 11, 5) == 15);
    assert(pixel(framebuffer, 11, 6) == 0);
    assert(pixel(framebuffer, 13, 5) == 15);

    memset(framebuffer, 0, sizeof(framebuffer));
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "!A", 0, 0, false);
    assert(pixel(framebuffer, 3, 1) == 0);
    assert(pixel(framebuffer, 5, 1) == 15);
    assert(display_text_width_for_font(&display_font_10_00c988, "!A") == 9);
}

static void test_font21_renderer(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    memset(framebuffer, 0xff, sizeof(framebuffer));

    display_text_draw_with_font(framebuffer, &display_font_21_00aba6, "!", 0, 0, false);
    for (uint16_t y = 0; y < 21; y++) {
        assert(pixel(framebuffer, 0, y) == 0);
    }
    assert(pixel(framebuffer, 1, 0) == 0);
    assert(pixel(framebuffer, 2, 0) == 0);
    assert(pixel(framebuffer, 3, 0) == 15);
    assert(pixel(framebuffer, 4, 0) == 15);
    assert(pixel(framebuffer, 5, 0) == 0);
    assert(pixel(framebuffer, 6, 0) == 0);
}

static void test_renderer_clipping(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    memset(framebuffer, 0xff, sizeof(framebuffer));

    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "!", 254, 62, false);
    assert(pixel(framebuffer, 253, 62) == 15);
    assert(pixel(framebuffer, 254, 62) == 0);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "!", 254, 63, false);
    assert(pixel(framebuffer, 254, 62) == 0);
}

int main(void) {
    test_font_tables_and_metrics();
    test_renderer_background_and_leading_column();
    test_renderer_inversion_and_variable_advance();
    test_font21_renderer();
    test_renderer_clipping();
    return 0;
}
