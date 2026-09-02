#include "display/prompt.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/high_torque_icon.h"
#include "display/text.h"

/**
 * @brief Defines prompt font, color, and icon layout values.
 *
 * The constants position prompt content in the local display framebuffer.
 */
enum {
    BITE_POINT_FONT_HEIGHT = 21,                     /**< Font21 glyph height in pixels. */
    BITE_POINT_FONT_SELECTOR = DISPLAY_TEXT_FONT_21, /**< Compatibility selector for Font21. */
    PROMPT_COLOR = 15,                               /**< Foreground grayscale value. */
    OVERLAY_LEFT = 2,                                /**< Filled overlay left coordinate. */
    OVERLAY_TOP = 16,                                /**< Filled overlay top coordinate. */
    OVERLAY_RIGHT = 253,     /**< Exclusive filled-overlay right endpoint. */
    OVERLAY_BOTTOM = 63,     /**< Exclusive filled-overlay bottom endpoint. */
    TORQUE_KEY_ICON_X = 123, /**< Torque Key icon left coordinate. */
    TORQUE_KEY_ICON_Y = 16,  /**< Torque Key icon top coordinate. */
};

static void draw_filled_overlay(DisplayFramebuffer framebuffer) {
    for (uint16_t row = OVERLAY_TOP; row < OVERLAY_BOTTOM; row++) {
        for (uint16_t column = OVERLAY_LEFT; column < OVERLAY_RIGHT; column++) {
            display_framebuffer_set_pixel(framebuffer, column, row, 0x0f);
        }
    }
}

static void draw_overlay_text(DisplayFramebuffer framebuffer, const char *text, uint16_t y,
                              bool primary) {
    const DisplayFont *font = &display_font_10_00c988;
    uint16_t width = display_text_width_for_font(font, text);
    if (width > DISPLAY_FRAMEBUFFER_WIDTH - 2u) {
        return;
    }
    uint16_t x = primary ? DISPLAY_FRAMEBUFFER_WIDTH / 2u - width / 2u
                         : (DISPLAY_FRAMEBUFFER_WIDTH - width) / 2u;
    display_text_draw_with_font(framebuffer, font, text, x, y, true);
}

/**
 * @brief Renders the force-output acknowledgement prompt.
 *
 * Clears the display and, while visible, draws the official white panel, high-torque icon, and
 * single-line Font10 acknowledgement text.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while the acknowledgement prompt owns the display.
 */
void display_prompt_render(DisplayFramebuffer framebuffer, bool visible) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }
    draw_filled_overlay(framebuffer);
    display_high_torque_icon_draw(framebuffer, TORQUE_KEY_ICON_X, TORQUE_KEY_ICON_Y, true);
    draw_overlay_text(framebuffer, "ATTENTION Enable torque?", 37, true);
}

/**
 * @brief Renders the Torque Key safety acknowledgement prompt.
 *
 * Clears the display and, while visible, draws the official white panel, inverted high-torque icon,
 * two inverted Font10 safety lines, and the non-inverted acknowledgement label.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while the Torque Key acknowledgement prompt owns the display.
 */
void display_prompt_render_torque_key(DisplayFramebuffer framebuffer, bool visible) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }

    draw_filled_overlay(framebuffer);
    display_high_torque_icon_draw(framebuffer, TORQUE_KEY_ICON_X, TORQUE_KEY_ICON_Y, true);
    draw_overlay_text(framebuffer, "CAUTION! Torque Key Inserted! Please read", 30, true);
    draw_overlay_text(framebuffer, "the manuals safety guidelines before use.", 40, false);
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, "OK", 120, 52, false);
}

/**
 * @brief Renders the active paddle bite-point value.
 *
 * Clears the framebuffer and, while visible, renders the unpadded decimal percentage
 * right-aligned in a centered three-character field.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while paddle bite-point adjustment owns the display.
 * @param[in] percent Current bite-point percentage from zero through one hundred.
 */
void display_prompt_render_bite_point(DisplayFramebuffer framebuffer, bool visible,
                                      uint8_t percent) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }

    char text[4] = {' ', ' ', '0', '\0'};
    if (percent >= 100) {
        text[0] = '1';
        text[1] = '0';
        text[2] = '0';
    } else if (percent >= 10) {
        text[1] = (char)('0' + percent / 10);
        text[2] = (char)('0' + percent % 10);
    } else {
        text[2] = (char)('0' + percent);
    }
    display_text_draw_centered(framebuffer, text,
                               (DISPLAY_FRAMEBUFFER_HEIGHT - BITE_POINT_FONT_HEIGHT) / 2,
                               BITE_POINT_FONT_SELECTOR, PROMPT_COLOR);
}

/**
 * @brief Detects acknowledgement input for an active display prompt.
 *
 * Latches any active input and reports acknowledgement only after all input is released. Hiding the
 * prompt clears a partially completed interaction.
 *
 * @param[in,out] prompt Prompt input latch.
 * @param[in] visible True while the acknowledgement prompt is displayed.
 * @param[in] input_active True while any caller-selected acknowledgement input is active.
 * @return True once after a latched input is released while the prompt remains visible.
 */
bool display_prompt_update(DisplayPrompt *prompt, bool visible, bool input_active) {
    if (!visible) {
        prompt->input_seen = false;
        return false;
    }
    if (input_active) {
        prompt->input_seen = true;
        return false;
    }
    if (!prompt->input_seen) {
        return false;
    }
    prompt->input_seen = false;
    return true;
}
