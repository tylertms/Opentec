#ifndef OPENTEC_BASE_USB_XBOX_GIP_VENDOR_TUNNEL_H
#define OPENTEC_BASE_USB_XBOX_GIP_VENDOR_TUNNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/output_command.h"

/**
 * @brief Decodes an Xbox GIP vendor tunnel packet.
 *
 * Accepts packet 0x0F with its 60-byte payload declaration. Marker 0x35 exposes the seven-byte
 * operating-mode command envelope, while marker 0x36 exposes the remaining 59-byte vendor command
 * payload through the shared USB command representation.
 *
 * @param[in] packet Received Xbox GIP endpoint packet.
 * @param[in] length Number of bytes available in @p packet.
 * @param[out] command Shared command representation for the tunneled payload.
 * @return `true` when the packet contains a supported vendor tunnel marker; otherwise `false`.
 */
bool usb_xbox_gip_vendor_tunnel_decode(const uint8_t *packet, size_t length,
                                       UsbOutputCommand *command);

#endif
