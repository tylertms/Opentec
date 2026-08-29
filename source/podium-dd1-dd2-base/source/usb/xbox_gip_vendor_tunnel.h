#ifndef OPENTEC_BASE_USB_XBOX_GIP_VENDOR_TUNNEL_H
#define OPENTEC_BASE_USB_XBOX_GIP_VENDOR_TUNNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/output_command.h"

bool usb_xbox_gip_vendor_tunnel_decode(const uint8_t *packet, size_t length,
                                       UsbOutputCommand *command);

#endif
