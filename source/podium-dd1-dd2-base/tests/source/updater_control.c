#include "usb/updater_control.h"

#include <assert.h>
#include <stdint.h>

static void test_defaults(void) {
    UsbUpdaterControl control;
    uint8_t encoded[USB_UPDATER_LINE_CODING_SIZE];
    usb_updater_control_init(&control);
    usb_updater_line_coding_encode(&control, encoded);
    const uint8_t expected[USB_UPDATER_LINE_CODING_SIZE] = {0x00, 0x4b, 0x00, 0x00, 0, 0, 8};
    for (uint8_t index = 0; index < sizeof(expected); index++) {
        assert(encoded[index] == expected[index]);
    }
    assert(control.control_line_state == 0);
}

static void test_updates(void) {
    UsbUpdaterControl control;
    const uint8_t line_coding[USB_UPDATER_LINE_CODING_SIZE] = {0x00, 0xc2, 0x01, 0x00, 2, 1, 7};
    usb_updater_control_init(&control);
    assert(usb_updater_line_coding_decode(&control, line_coding, sizeof(line_coding)));
    assert(control.baud_rate == 115200);
    assert(control.stop_bits == 2);
    assert(control.parity == 1);
    assert(control.data_bits == 7);
    usb_updater_control_set_lines(&control, 0x03);
    assert(control.control_line_state == 0x03);
}

static void test_rejections(void) {
    UsbUpdaterControl control;
    uint8_t line_coding[USB_UPDATER_LINE_CODING_SIZE] = {0};
    usb_updater_control_init(&control);
    assert(!usb_updater_line_coding_decode(0, line_coding, sizeof(line_coding)));
    assert(!usb_updater_line_coding_decode(&control, 0, sizeof(line_coding)));
    assert(!usb_updater_line_coding_decode(&control, line_coding, sizeof(line_coding) - 1));
}

int main(void) {
    test_defaults();
    test_updates();
    test_rejections();
    return 0;
}
