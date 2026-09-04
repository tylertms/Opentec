#ifndef OPENTEC_BASE_USB_OUTPUT_COMMAND_H
#define OPENTEC_BASE_USB_OUTPUT_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"

/** @brief Classified output-command payload kinds. */
typedef enum {
    USB_OUTPUT_COMMAND_SHORT,           /**< Seven-byte short command payload. */
    USB_OUTPUT_COMMAND_VENDOR_TRANSFER, /**< Sixty-three-byte vendor-transfer payload. */
} UsbOutputCommandKind;

/** @brief Output-command kind and payload view extracted from a USB report. */
typedef struct {
    UsbOutputCommandKind kind; /**< Classified command kind. */
    const uint8_t *payload;    /**< Pointer to the command payload within the source report. */
    uint8_t length;            /**< Number of payload bytes. */
} UsbOutputCommand;

/**
 * @brief Decodes a supported USB HID output report.
 *
 * Validates the report type, report identifier, and length, then exposes either the seven-byte
 * short command payload or the sixty-three-byte vendor-transfer payload without copying it. Native
 * reports 1 and 2 accept an eight-byte interrupt-transfer payload with the identifier repeated in
 * data[0]. Native report 2 also accepts its seven-byte control-transfer payload, whose identifier is
 * carried in report_id. Both PlayStation report identifiers 0x30 and 0x34 expose all 63 bytes
 * following the identifier.
 *
 * @param[in] report USB HID output report to classify.
 * @param[out] command Destination for the command kind and payload view.
 * @return True when report and command are non-null and the report matches a supported layout;
 * otherwise false.
 */
bool usb_output_command_decode(const UsbDeviceOutputReport *report, UsbOutputCommand *command);

#endif
