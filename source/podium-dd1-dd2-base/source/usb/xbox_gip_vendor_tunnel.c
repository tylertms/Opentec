#include "usb/xbox_gip_vendor_tunnel.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Private framing values used by the Xbox GIP vendor tunnel decoder. */
enum {
    XBOX_GIP_VENDOR_PACKET = 0x0f,           /**< Vendor tunnel packet identifier. */
    XBOX_GIP_VENDOR_PACKET_SIZE = 64,        /**< Required vendor tunnel packet length. */
    XBOX_GIP_VENDOR_PAYLOAD_SIZE_OFFSET = 3, /**< Payload-size field offset. */
    XBOX_GIP_VENDOR_PAYLOAD_SIZE = 60,       /**< Declared vendor tunnel payload size. */
    XBOX_GIP_VENDOR_PAYLOAD_OFFSET = 4,      /**< Payload start offset. */
    XBOX_GIP_VENDOR_OPERATING_MARKER = 0x35, /**< Operating-mode command marker. */
    XBOX_GIP_VENDOR_COMMAND_MARKER = 0x36,   /**< Vendor command marker. */
    XBOX_GIP_VENDOR_COMMAND_OFFSET = 1,      /**< Command data offset within the tunnel payload. */
    XBOX_GIP_OPERATING_COMMAND_SIZE = 7,     /**< Operating-mode command envelope length. */
};

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
