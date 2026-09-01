#ifndef OPENTEC_BASE_USB_PODIUM_REPORT_DESCRIPTOR_H
#define OPENTEC_BASE_USB_PODIUM_REPORT_DESCRIPTOR_H

#include <stddef.h>
#include <stdint.h>

/** @brief Size of the native Podium HID report descriptor. */
enum {
    USB_PODIUM_REPORT_DESCRIPTOR_SIZE = 296 /**< Descriptor size in bytes. */
};

/**
 * @brief Encodes the native Podium HID report descriptor.
 *
 * Writes the primary wheel, extended wheel, and bidirectional transfer collections into the
 * caller-owned output buffer in host-visible order.
 *
 * @param[out] output Buffer with room for USB_PODIUM_REPORT_DESCRIPTOR_SIZE bytes.
 * @return USB_PODIUM_REPORT_DESCRIPTOR_SIZE after the descriptor is written.
 */
size_t usb_podium_report_descriptor_encode(uint8_t output[USB_PODIUM_REPORT_DESCRIPTOR_SIZE]);

#endif
