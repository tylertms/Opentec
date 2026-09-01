#include "usb/xbox_gip_discovery.h"

#include <stdint.h>

/** @brief Request identifiers recognized during Xbox GIP discovery. */
enum {
    XBOX_GIP_METADATA_REQUEST = 4, /**< Metadata-download request identifier. */
    XBOX_GIP_SESSION_COMMAND = 5,  /**< Session-command request identifier. */
};

void usb_xbox_gip_discovery_init(UsbXboxGipDiscovery *discovery) {
    *discovery = (UsbXboxGipDiscovery){0};
}

UsbXboxGipDiscoveryAction usb_xbox_gip_discovery_poll(UsbXboxGipDiscovery *discovery,
                                                      uint8_t request_id, uint32_t now) {
    if (discovery->phase == USB_XBOX_GIP_DISCOVERY_SEND_DIGEST) {
        discovery->deadline = now + USB_XBOX_GIP_DISCOVERY_TIMEOUT_MS;
        discovery->phase = USB_XBOX_GIP_DISCOVERY_WAIT_FOR_REQUEST;
        return USB_XBOX_GIP_DISCOVERY_DIGEST;
    }

    if (request_id == XBOX_GIP_METADATA_REQUEST) {
        return USB_XBOX_GIP_DISCOVERY_METADATA;
    }
    if (request_id == XBOX_GIP_SESSION_COMMAND) {
        return USB_XBOX_GIP_DISCOVERY_SESSION_COMMAND;
    }
    if ((int32_t)(now - discovery->deadline) >= 0) {
        discovery->phase = USB_XBOX_GIP_DISCOVERY_SEND_DIGEST;
    }
    return USB_XBOX_GIP_DISCOVERY_IDLE;
}
