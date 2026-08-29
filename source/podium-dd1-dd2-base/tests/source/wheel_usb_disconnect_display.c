#include <assert.h>

#include "wheel/usb_disconnect_display.h"

static void test_shows_usb_without_changing_auxiliary_output(void) {
    UsbDisconnectDisplay display;
    WheelDisplayOutput output = {
        .glyphs = {1, 2, 3},
        .auxiliary = 0xa5,
        .third_glyph_marker = true,
    };
    usb_disconnect_display_init(&display);

    assert(usb_disconnect_display_update(&display, true, &output));
    assert(output.glyphs[0] == 0x3e);
    assert(output.glyphs[1] == 0x6d);
    assert(output.glyphs[2] == 0x7c);
    assert(output.auxiliary == 0xa5);
    assert(output.third_glyph_marker);
    assert(display.owns_output);
    assert(!usb_disconnect_display_update(&display, true, &output));
}

static void test_clears_owned_glyphs_once(void) {
    UsbDisconnectDisplay display;
    WheelDisplayOutput output = {
        .auxiliary = 0x5a,
        .third_glyph_marker = true,
    };
    usb_disconnect_display_init(&display);
    usb_disconnect_display_update(&display, true, &output);

    assert(usb_disconnect_display_update(&display, false, &output));
    assert(output.glyphs[0] == 0);
    assert(output.glyphs[1] == 0);
    assert(output.glyphs[2] == 0);
    assert(output.auxiliary == 0x5a);
    assert(output.third_glyph_marker);
    assert(!display.owns_output);
    assert(!usb_disconnect_display_update(&display, false, &output));
}

int main(void) {
    test_shows_usb_without_changing_auxiliary_output();
    test_clears_owned_glyphs_once();
    return 0;
}
