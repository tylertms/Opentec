#include "usb/xbox_gip_vendor_tunnel.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    XBOX_GIP_VENDOR_PACKET = 0x0f,
    XBOX_GIP_VENDOR_PACKET_SIZE = 64,
    XBOX_GIP_VENDOR_PAYLOAD_SIZE_OFFSET = 3,
    XBOX_GIP_VENDOR_PAYLOAD_SIZE = 60,
    XBOX_GIP_VENDOR_PAYLOAD_OFFSET = 4,
    XBOX_GIP_VENDOR_OPERATING_MARKER = 0x35,
    XBOX_GIP_VENDOR_COMMAND_MARKER = 0x36,
    XBOX_GIP_VENDOR_COMMAND_OFFSET = 1,
    XBOX_GIP_OPERATING_COMMAND_SIZE = 7,
};

/**
 * @brief Decodes an Xbox GIP vendor tunnel packet.
 *
 * Accepts packet 0F with its 60-byte payload declaration. Marker 35 exposes the seven-byte
 * operating-mode command envelope, while marker 36 exposes the remaining 59-byte vendor command
 * payload through the shared USB command representation.
 *
 * @param[in] packet Received Xbox GIP endpoint packet.
 * @param[in] length Number of available packet bytes.
 * @param[out] command Shared command representation for the tunneled payload.
 * @return True when the packet contains a supported vendor tunnel marker.
 */
bool usb_xbox_gip_vendor_tunnel_decode(const uint8_t *packet, size_t length,
                                       UsbOutputCommand *command) {
    if (packet == NULL || command == NULL || length < XBOX_GIP_VENDOR_PACKET_SIZE ||
        packet[0] != XBOX_GIP_VENDOR_PACKET ||
        packet[XBOX_GIP_VENDOR_PAYLOAD_SIZE_OFFSET] != XBOX_GIP_VENDOR_PAYLOAD_SIZE) {
        return false;
    }

    const uint8_t *payload = packet + XBOX_GIP_VENDOR_PAYLOAD_OFFSET;
    if (payload[0] == XBOX_GIP_VENDOR_OPERATING_MARKER) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_SHORT,
            .payload = payload + XBOX_GIP_VENDOR_COMMAND_OFFSET,
            .length = XBOX_GIP_OPERATING_COMMAND_SIZE,
        };
        return true;
    }
    if (payload[0] == XBOX_GIP_VENDOR_COMMAND_MARKER) {
        *command = (UsbOutputCommand){
            .kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER,
            .payload = payload + XBOX_GIP_VENDOR_COMMAND_OFFSET,
            .length = XBOX_GIP_VENDOR_PAYLOAD_SIZE - XBOX_GIP_VENDOR_COMMAND_OFFSET,
        };
        return true;
    }
    return false;
}
