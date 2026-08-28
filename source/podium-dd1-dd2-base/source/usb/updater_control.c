#include "usb/updater_control.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the motor updater serial controls.
 *
 * Selects the 19,200-baud, eight-data-bit, no-parity, one-stop-bit line coding and clears the
 * control-line state.
 *
 * @param[out] control Updater serial control state.
 */
void usb_updater_control_init(UsbUpdaterControl *control) {
    if (control == 0) {
        return;
    }
    *control = (UsbUpdaterControl){
        .baud_rate = 19200,
        .data_bits = 8,
    };
}

/**
 * @brief Encodes the motor updater serial line coding.
 *
 * Writes the baud rate in little-endian order followed by the stop-bit, parity, and data-bit
 * fields used by the seven-byte CDC line-coding response.
 *
 * @param[in] control Updater serial control state.
 * @param[out] output Destination for the seven-byte line coding.
 */
void usb_updater_line_coding_encode(const UsbUpdaterControl *control,
                                    uint8_t output[USB_UPDATER_LINE_CODING_SIZE]) {
    output[0] = (uint8_t)control->baud_rate;
    output[1] = (uint8_t)(control->baud_rate >> 8);
    output[2] = (uint8_t)(control->baud_rate >> 16);
    output[3] = (uint8_t)(control->baud_rate >> 24);
    output[4] = control->stop_bits;
    output[5] = control->parity;
    output[6] = control->data_bits;
}

/**
 * @brief Decodes the motor updater serial line coding.
 *
 * Replaces all serial format fields from a complete seven-byte CDC line-coding payload.
 *
 * @param[in,out] control Updater serial control state.
 * @param[in] data CDC line-coding payload.
 * @param[in] length Payload length in bytes.
 * @return True when a complete payload was decoded; otherwise false.
 */
bool usb_updater_line_coding_decode(UsbUpdaterControl *control, const uint8_t *data,
                                    uint8_t length) {
    if (control == 0 || data == 0 || length != USB_UPDATER_LINE_CODING_SIZE) {
        return false;
    }
    control->baud_rate = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
                         (uint32_t)data[3] << 24;
    control->stop_bits = data[4];
    control->parity = data[5];
    control->data_bits = data[6];
    return true;
}

/**
 * @brief Stores the motor updater serial control-line state.
 *
 * Retains the low request byte supplied with the CDC control-line-state command.
 *
 * @param[in,out] control Updater serial control state.
 * @param[in] state Low control-line-state request byte.
 */
void usb_updater_control_set_lines(UsbUpdaterControl *control, uint8_t state) {
    if (control != 0) {
        control->control_line_state = state;
    }
}
