#ifndef OPENTEC_BASE_DISPLAY_FONT_H
#define OPENTEC_BASE_DISPLAY_FONT_H

#include <stdint.h>

/**
 * @brief Describes one official one-bit display glyph.
 *
 * The bitmap stores row-major MSB-first rows. The format value is the official value 8, and the
 * width is the horizontal advance because the renderer does not add inter-glyph spacing. The
 * packed record is seven bytes on XC16, matching the original display data layout.
 */
#if defined(__GNUC__)
#define OPENTEC_DISPLAY_PACKED __attribute__((packed))
#else
#define OPENTEC_DISPLAY_PACKED
#endif

typedef struct DisplayGlyph DisplayGlyph;
typedef struct DisplayFontCharacter DisplayFontCharacter;

struct OPENTEC_DISPLAY_PACKED DisplayGlyph {
    const uint8_t *bitmap;
    uint16_t width;
    uint16_t height;
    uint8_t format;
};

/**
 * @brief Associates one printable character with its display glyph.
 *
 * The packed record is three bytes on XC16, matching the original display data layout.
 */
struct OPENTEC_DISPLAY_PACKED DisplayFontCharacter {
    uint8_t character;
    const DisplayGlyph *glyph;
};

/**
 * @brief Describes an official display font character table.
 *
 * The packed record is three bytes on XC16, matching the original display data layout.
 */
typedef struct OPENTEC_DISPLAY_PACKED {
    uint8_t count;
    const DisplayFontCharacter *characters;
} DisplayFont;

#if defined(__XC16__)
typedef char display_glyph_binary_size[(sizeof(DisplayGlyph) == 7) ? 1 : -1];
typedef char display_font_character_binary_size[(sizeof(DisplayFontCharacter) == 3) ? 1 : -1];
typedef char display_font_binary_size[(sizeof(DisplayFont) == 3) ? 1 : -1];
#endif

/**
 * @brief Official Font10 object at binary address 0x00c988.
 *
 * Its bitmap bytes begin at 0x00cd9f and contain 981 bytes copied from the 3.9.1.1 display image.
 */
extern const DisplayFont display_font_10_00c988;
/**
 * @brief Official Font21 object at binary address 0x00aba6.
 *
 * Its bitmap bytes begin at 0x00afbd and contain 3529 bytes copied from the 3.9.1.1 display image.
 */
extern const DisplayFont display_font_21_00aba6;

#undef OPENTEC_DISPLAY_PACKED

#endif
