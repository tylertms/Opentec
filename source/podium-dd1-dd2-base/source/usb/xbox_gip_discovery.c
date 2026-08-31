#include "usb/xbox_gip_discovery.h"

#include <stdint.h>

enum {
    XBOX_GIP_METADATA_REQUEST = 4,
    XBOX_GIP_SESSION_COMMAND = 5,
};

/**
 * @brief Initializes Xbox GIP discovery.
 *
 * Selects the digest phase so the next service cycle announces the device before accepting a
 * metadata or session request.
 *
 * @param[out] discovery Discovery state to initialize.
 */
void usb_xbox_gip_discovery_init(UsbXboxGipDiscovery *discovery) {
    *discovery = (UsbXboxGipDiscovery){0};
}

/**
 * @brief Advances Xbox GIP discovery.
 *
 * Requests a digest, waits 500 milliseconds for metadata request 4 or session request 5, and
 * returns to the digest phase only after the deadline has passed.
 *
 * @param[in,out] discovery Active discovery state.
 * @param[in] request_id First byte of the received request packet.
 * @param[in] now Current monotonic time in milliseconds.
 * @return Discovery action for the current service cycle.
 */
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
