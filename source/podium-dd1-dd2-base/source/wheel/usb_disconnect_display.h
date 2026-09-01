#ifndef OPENTEC_BASE_WHEEL_USB_DISCONNECT_DISPLAY_H
#define OPENTEC_BASE_WHEEL_USB_DISCONNECT_DISPLAY_H

#include <stdbool.h>

#include "wheel/display_output.h"

/** @brief Ownership state for the attached-wheel USB disconnect display. */
typedef struct {
    bool owns_output; /**< True while this presentation owns the output glyphs. */
} UsbDisconnectDisplay;

/**
 * @brief Initializes USB disconnect display state.
 *
 * Clears display ownership so the first visible update can claim the output.
 *
 * @param[out] display Disconnect display state to initialize.
 */
void usb_disconnect_display_init(UsbDisconnectDisplay *display);

/**
 * @brief Updates the USB disconnect display.
 *
 * Shows the USB glyphs while visible and clears only glyphs owned by this presentation when it is
 * hidden.
 *
 * @param[in,out] display Disconnect display ownership state.
 * @param[in] visible True to show the USB disconnect label, or false to release it.
 * @param[in,out] output Attached-wheel display output to update.
 * @return True when any output glyph changed; otherwise false.
 */
bool usb_disconnect_display_update(UsbDisconnectDisplay *display, bool visible,
                                   WheelDisplayOutput *output);

#endif
