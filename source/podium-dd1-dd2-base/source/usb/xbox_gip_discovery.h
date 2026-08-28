#ifndef OPENTEC_BASE_USB_XBOX_GIP_DISCOVERY_H
#define OPENTEC_BASE_USB_XBOX_GIP_DISCOVERY_H

#include <stdint.h>

enum { USB_XBOX_GIP_DISCOVERY_TIMEOUT_MS = 500 };

typedef enum {
    USB_XBOX_GIP_DISCOVERY_SEND_DIGEST,
    USB_XBOX_GIP_DISCOVERY_WAIT_FOR_REQUEST,
} UsbXboxGipDiscoveryPhase;

typedef enum {
    USB_XBOX_GIP_DISCOVERY_IDLE,
    USB_XBOX_GIP_DISCOVERY_DIGEST,
    USB_XBOX_GIP_DISCOVERY_METADATA,
    USB_XBOX_GIP_DISCOVERY_SESSION_COMMAND,
} UsbXboxGipDiscoveryAction;

typedef struct {
    uint32_t deadline;
    UsbXboxGipDiscoveryPhase phase;
} UsbXboxGipDiscovery;

void usb_xbox_gip_discovery_init(UsbXboxGipDiscovery *discovery);
UsbXboxGipDiscoveryAction usb_xbox_gip_discovery_poll(UsbXboxGipDiscovery *discovery,
                                                      uint8_t request_id, uint32_t now);

#endif
