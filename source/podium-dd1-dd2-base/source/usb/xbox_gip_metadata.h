#ifndef OPENTEC_BASE_USB_XBOX_GIP_METADATA_H
#define OPENTEC_BASE_USB_XBOX_GIP_METADATA_H

#include <stdint.h>

enum { USB_XBOX_GIP_METADATA_SIZE = 449 };

void usb_xbox_gip_metadata_encode(uint8_t output[USB_XBOX_GIP_METADATA_SIZE]);

#endif
