#ifndef OPENTEC_BASE_USB_PODIUM_REPORT_DESCRIPTOR_H
#define OPENTEC_BASE_USB_PODIUM_REPORT_DESCRIPTOR_H

#include <stddef.h>
#include <stdint.h>

enum { USB_PODIUM_REPORT_DESCRIPTOR_SIZE = 296 };

size_t usb_podium_report_descriptor_encode(uint8_t output[USB_PODIUM_REPORT_DESCRIPTOR_SIZE]);

#endif
