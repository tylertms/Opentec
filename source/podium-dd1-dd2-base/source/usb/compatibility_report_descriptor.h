#ifndef OPENTEC_BASE_USB_COMPATIBILITY_REPORT_DESCRIPTOR_H
#define OPENTEC_BASE_USB_COMPATIBILITY_REPORT_DESCRIPTOR_H

#include <stddef.h>
#include <stdint.h>

#include "usb/input_report.h"

/** @brief Sizes of supported compatibility HID report descriptors. */
enum {
    USB_FANATEC_COMPATIBILITY_REPORT_DESCRIPTOR_SIZE =
        133, /**< Fanatec compatibility report descriptor size in bytes. */
    USB_DRIVING_FORCE_EX_REPORT_DESCRIPTOR_SIZE =
        130, /**< Driving Force EX report descriptor size in bytes. */
    USB_DRIVING_FORCE_PRO_REPORT_DESCRIPTOR_SIZE =
        97,                               /**< Driving Force Pro report descriptor size in bytes. */
    USB_G27_REPORT_DESCRIPTOR_SIZE = 133, /**< G27 report descriptor size in bytes. */
    USB_COMPATIBILITY_REPORT_DESCRIPTOR_MAX_SIZE =
        133, /**< Largest supported compatibility report descriptor size in bytes. */
};

/**
 * @brief Encodes the HID report descriptor for a compatibility operating mode.
 *
 * Selects the Fanatec compatibility, Driving Force EX, Driving Force Pro, or G27 descriptor and
 * copies it to the caller's bounded output buffer.
 *
 * @param[in] mode Compatibility operating-mode selector.
 * @param[out] output Buffer that receives the selected report descriptor.
 * @param[in] capacity Available bytes in the output buffer.
 * @return Encoded descriptor length, or zero when the mode, output pointer, or capacity is invalid.
 */
size_t usb_compatibility_report_descriptor_encode(
    UsbInputReportMode mode, uint8_t output[USB_COMPATIBILITY_REPORT_DESCRIPTOR_MAX_SIZE],
    size_t capacity);

#endif
