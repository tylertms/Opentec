#ifndef OPENTEC_BASE_USB_UPDATER_CONTROL_H
#define OPENTEC_BASE_USB_UPDATER_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/** @brief USB CDC line-coding constants. */
enum { USB_UPDATER_LINE_CODING_SIZE = 7 /**< Number of bytes in the line-coding structure. */ };

/** @brief Serial and control-line state exposed by the updater USB interface. */
typedef struct {
    uint32_t baud_rate;         /**< Serial baud rate in bits per second. */
    uint8_t control_line_state; /**< Low byte of the CDC control-line state request. */
    uint8_t stop_bits;          /**< CDC stop-bit encoding. */
    uint8_t parity;             /**< CDC parity encoding. */
    uint8_t data_bits;          /**< Number of data bits in each serial character. */
} UsbUpdaterControl;

/**
 * @brief Initializes updater serial control state.
 *
 * Sets the default 19,200-baud, eight-data-bit, no-parity, one-stop-bit line coding and clears
 * the control-line state.
 *
 * @param[out] control Updater serial control state to initialize.
 */
void usb_updater_control_init(UsbUpdaterControl *control);

/**
 * @brief Encodes updater serial line coding.
 *
 * Writes the baud rate in little-endian order followed by the stop-bit, parity, and data-bit
 * fields in the USB CDC line-coding representation.
 *
 * @param[in] control Updater serial control state to encode.
 * @param[out] output Seven-byte CDC line-coding destination.
 */
void usb_updater_line_coding_encode(const UsbUpdaterControl *control,
                                    uint8_t output[USB_UPDATER_LINE_CODING_SIZE]);

/**
 * @brief Decodes updater serial line coding.
 *
 * Replaces all serial format fields from a complete seven-byte USB CDC line-coding payload.
 *
 * @param[in,out] control Updater serial control state to update.
 * @param[in] data CDC line-coding payload.
 * @param[in] length Number of bytes in @p data.
 * @return `true` when exactly seven bytes were decoded; otherwise `false`.
 */
bool usb_updater_line_coding_decode(UsbUpdaterControl *control, const uint8_t *data,
                                    uint8_t length);

/**
 * @brief Stores updater serial control-line state.
 *
 * Retains the low request byte supplied with the USB CDC control-line-state command.
 *
 * @param[in,out] control Updater serial control state to update.
 * @param[in] state Low byte of the control-line-state request.
 */
void usb_updater_control_set_lines(UsbUpdaterControl *control, uint8_t state);

#endif
