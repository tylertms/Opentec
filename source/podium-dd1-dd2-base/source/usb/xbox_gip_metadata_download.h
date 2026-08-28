#ifndef OPENTEC_BASE_USB_XBOX_GIP_METADATA_DOWNLOAD_H
#define OPENTEC_BASE_USB_XBOX_GIP_METADATA_DOWNLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/xbox_gip_metadata.h"

enum {
    USB_XBOX_GIP_METADATA_REPORT_ID = 4,
    USB_XBOX_GIP_METADATA_PACKET_SIZE = 64,
    USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE = 13,
};

typedef struct {
    uint16_t offset;
    uint8_t sequence;
    uint8_t continuation_count;
    bool awaiting_acknowledgement;
    bool complete;
} UsbXboxGipMetadataDownload;

void usb_xbox_gip_metadata_download_init(UsbXboxGipMetadataDownload *download, uint8_t sequence);
bool usb_xbox_gip_metadata_download_next(UsbXboxGipMetadataDownload *download,
                                         const uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE],
                                         uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE],
                                         uint8_t *packet_length);
bool usb_xbox_gip_metadata_download_acknowledge(
    UsbXboxGipMetadataDownload *download,
    const uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE]);

#endif
