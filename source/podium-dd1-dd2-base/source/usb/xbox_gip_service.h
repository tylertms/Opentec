#ifndef OPENTEC_BASE_USB_XBOX_GIP_SERVICE_H
#define OPENTEC_BASE_USB_XBOX_GIP_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/xbox_gip_discovery.h"
#include "usb/xbox_gip_metadata_download.h"
#include "usb/xbox_gip_session.h"

typedef struct {
    BoardVariant variant;
    uint8_t wheel_mode;
    const uint8_t *digest;
    const uint8_t *metadata;
} UsbXboxGipServiceIdentity;

typedef struct {
    UsbXboxGipDiscovery discovery;
    UsbXboxGipMetadataDownload metadata_download;
    UsbXboxGipSession session;
    uint8_t next_sequence;
    bool metadata_pending;
    bool metadata_active;
} UsbXboxGipService;

typedef struct {
    UsbXboxGipSessionAction session_actions;
    uint8_t response_length;
    bool application_output;
} UsbXboxGipServiceResult;

void usb_xbox_gip_service_init(UsbXboxGipService *service);
UsbXboxGipServiceResult
usb_xbox_gip_service_poll(UsbXboxGipService *service, const UsbXboxGipServiceIdentity *identity,
                          const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE], uint32_t now,
                          uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE]);

#endif
