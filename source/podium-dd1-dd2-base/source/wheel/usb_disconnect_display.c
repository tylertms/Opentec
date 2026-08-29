#include "wheel/usb_disconnect_display.h"

#include <stdbool.h>

#include "wheel/display_output.h"

enum {
    GLYPH_U = 0x3e,
    GLYPH_S = 0x6d,
    GLYPH_B = 0x7c,
};

/**
 * @brief Initializes the attached-wheel USB disconnect display.
 *
 * Releases ownership of the three-glyph display so existing wheel output remains unchanged.
 *
 * @param[out] display Persistent disconnect display state to initialize.
 */
void usb_disconnect_display_init(UsbDisconnectDisplay *display) {
    *display = (UsbDisconnectDisplay){0};
}

/**
 * @brief Updates the attached-wheel USB disconnect presentation.
 *
 * Displays the three-glyph USB label while requested. When the presentation ends, clears the
 * glyphs it owned without changing auxiliary output or the third-glyph marker.
 *
 * @param[in,out] display Persistent ownership state for the disconnect presentation.
 * @param[in] visible True when the USB disconnect label must be displayed.
 * @param[in,out] output Current attached-wheel display output.
 * @return True when any display glyph changed.
 */
bool usb_disconnect_display_update(UsbDisconnectDisplay *display, bool visible,
                                   WheelDisplayOutput *output) {
    const uint8_t glyphs[WHEEL_DISPLAY_GLYPH_COUNT] = {GLYPH_U, GLYPH_S, GLYPH_B};
    if (visible) {
        bool changed = output->glyphs[0] != glyphs[0] || output->glyphs[1] != glyphs[1] ||
                       output->glyphs[2] != glyphs[2];
        for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
            output->glyphs[index] = glyphs[index];
        }
        display->owns_output = true;
        return changed;
    }

    if (!display->owns_output) {
        return false;
    }

    bool changed = output->glyphs[0] != 0 || output->glyphs[1] != 0 || output->glyphs[2] != 0;
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        output->glyphs[index] = 0;
    }
    display->owns_output = false;
    return changed;
}
