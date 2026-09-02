#include "display/text.h"

#include <stddef.h>
#include <stdint.h>

#include "display/framebuffer.h"

static const DisplayFont *font_for_scale(uint8_t scale) {
    if (scale == DISPLAY_TEXT_FONT_10) {
        return &display_font_10_00c988;
    }
    if (scale == DISPLAY_TEXT_FONT_21) {
        return &display_font_21_00aba6;
    }
    return NULL;
}

static const DisplayGlyph *find_glyph(const DisplayFont *font, uint8_t value) {
    uint16_t index = 0;
    if (font == NULL || font->characters == NULL || font->count == 0) {
        return NULL;
    }
    index = value > 0x1fu ? value - 0x20u : value;
    if (index >= font->count) {
        return NULL;
    }
    return font->characters[index].glyph;
}

static bool glyph_is_drawable(const DisplayGlyph *glyph) {
    return glyph != NULL && glyph->bitmap != NULL && glyph->width != 0 && glyph->height != 0 &&
           glyph->width <= DISPLAY_FRAMEBUFFER_WIDTH && glyph->height <= DISPLAY_FRAMEBUFFER_HEIGHT;
}

uint16_t display_text_width_for_font(const DisplayFont *font, const char *text) {
    uint16_t width = 0;
    if (font == NULL || text == NULL) {
        return 0;
    }
    for (size_t index = 0; text[index] != '\0'; index++) {
        const DisplayGlyph *glyph = find_glyph(font, (uint8_t)text[index]);
        if (glyph_is_drawable(glyph)) {
            width = (uint16_t)(width + glyph->width);
        }
    }
    return width;
}

static void set_text_pixel(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y, uint8_t value) {
    display_framebuffer_set_pixel(framebuffer, x, y, value);
}

static void draw_text(DisplayFramebuffer framebuffer, const DisplayFont *font, const char *text,
                      uint16_t x, uint16_t y, bool invert, uint8_t foreground) {
    uint16_t cursor = (uint16_t)(x + 1u);
    uint8_t background = invert ? 0x0f : 0;
    if (font == NULL || text == NULL) {
        return;
    }
    for (size_t index = 0; text[index] != '\0'; index++) {
        const DisplayGlyph *glyph = find_glyph(font, (uint8_t)text[index]);
        if (!glyph_is_drawable(glyph)) {
            continue;
        }
        for (uint16_t row = 0; row < glyph->height; row++) {
            set_text_pixel(framebuffer, x, (uint16_t)(y + row), background);
            for (uint16_t column = 0; column < glyph->width; column++) {
                uint16_t bitmap_index =
                    (uint16_t)(((glyph->width + 7u) >> 3u) * row + (column >> 3u));
                bool lit = (glyph->bitmap[bitmap_index] & (uint8_t)(0x80u >> (column & 7u))) != 0;
                uint8_t pixel = lit ? foreground : 0;
                if (invert) {
                    pixel = (uint8_t)~pixel;
                }
                set_text_pixel(framebuffer, (uint16_t)(cursor + column), (uint16_t)(y + row),
                               pixel);
            }
        }
        cursor += glyph->width;
    }
}

void display_text_draw_with_font(DisplayFramebuffer framebuffer, const DisplayFont *font,
                                 const char *text, uint16_t x, uint16_t y, bool invert) {
    draw_text(framebuffer, font, text, x, y, invert, 0x0f);
}

void display_text_draw_centered_with_font(DisplayFramebuffer framebuffer, const DisplayFont *font,
                                          const char *text, uint16_t y, bool invert) {
    uint16_t width = display_text_width_for_font(font, text);
    uint16_t x = width < DISPLAY_FRAMEBUFFER_WIDTH ? (DISPLAY_FRAMEBUFFER_WIDTH - width) / 2u : 0;
    display_text_draw_with_font(framebuffer, font, text, x, y, invert);
}

uint16_t display_text_width(const char *text, uint8_t scale) {
    return display_text_width_for_font(font_for_scale(scale), text);
}

void display_text_draw(DisplayFramebuffer framebuffer, const char *text, uint16_t x, uint16_t y,
                       uint8_t scale, uint8_t color) {
    draw_text(framebuffer, font_for_scale(scale), text, x, y, false, color & 0x0f);
}

void display_text_draw_centered(DisplayFramebuffer framebuffer, const char *text, uint16_t y,
                                uint8_t scale, uint8_t color) {
    const DisplayFont *font = font_for_scale(scale);
    uint16_t width = display_text_width_for_font(font, text);
    uint16_t x = width < DISPLAY_FRAMEBUFFER_WIDTH ? (DISPLAY_FRAMEBUFFER_WIDTH - width) / 2u : 0;
    draw_text(framebuffer, font, text, x, y, false, color & 0x0f);
}
