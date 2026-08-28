#ifndef OPENTEC_BASE_USB_UPDATER_CONTROL_H
#define OPENTEC_BASE_USB_UPDATER_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

enum { USB_UPDATER_LINE_CODING_SIZE = 7 };

typedef struct {
    uint32_t baud_rate;
    uint8_t control_line_state;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t data_bits;
} UsbUpdaterControl;

void usb_updater_control_init(UsbUpdaterControl *control);
void usb_updater_line_coding_encode(const UsbUpdaterControl *control,
                                    uint8_t output[USB_UPDATER_LINE_CODING_SIZE]);
bool usb_updater_line_coding_decode(UsbUpdaterControl *control, const uint8_t *data,
                                    uint8_t length);
void usb_updater_control_set_lines(UsbUpdaterControl *control, uint8_t state);

#endif
