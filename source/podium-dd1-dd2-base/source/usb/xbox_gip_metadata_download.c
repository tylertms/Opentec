#include "usb/xbox_gip_metadata_download.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_download.h"
#include "usb/xbox_gip_metadata.h"

void usb_xbox_gip_metadata_download_init(UsbXboxGipMetadataDownload *download, uint8_t sequence) {
    usb_feature_download_init(download, USB_XBOX_GIP_METADATA_REPORT_ID, sequence,
                              USB_XBOX_GIP_METADATA_SIZE, false);
}

uint8_t usb_xbox_gip_metadata_download_next(UsbXboxGipMetadataDownload *download,
                                            const uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE],
                                            uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    return usb_feature_download_next(download, metadata, packet);
}

bool usb_xbox_gip_metadata_download_acknowledge(
    UsbXboxGipMetadataDownload *download,
    const uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE]) {
    return usb_feature_download_acknowledge(download, acknowledgement);
}
