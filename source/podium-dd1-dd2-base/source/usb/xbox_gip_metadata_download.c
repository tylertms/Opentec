#include "usb/xbox_gip_metadata_download.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_download.h"
#include "usb/xbox_gip_metadata.h"

/**
 * @brief Starts an Xbox GIP metadata download.
 *
 * Configures feature report 4 to transfer the complete metadata document without a synthetic
 * leading byte.
 *
 * @param[out] download Download state to initialize.
 * @param[in] sequence Sequence value carried by the metadata packets.
 */
void usb_xbox_gip_metadata_download_init(UsbXboxGipMetadataDownload *download, uint8_t sequence) {
    usb_feature_download_init(download, USB_XBOX_GIP_METADATA_REPORT_ID, sequence,
                              USB_XBOX_GIP_METADATA_SIZE, false);
}

/**
 * @brief Builds the next Xbox GIP metadata packet.
 *
 * Delegates compact, segmented, acknowledgement-boundary, and terminal framing to the shared
 * feature-download state machine.
 *
 * @param[in,out] download Active metadata download state.
 * @param[in] metadata Encoded metadata document.
 * @param[out] packet Destination for the next metadata packet.
 * @return Number of packet bytes produced, or zero while waiting or after completion.
 */
uint8_t usb_xbox_gip_metadata_download_next(UsbXboxGipMetadataDownload *download,
                                            const uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE],
                                            uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    return usb_feature_download_next(download, metadata, packet);
}

/**
 * @brief Accepts an Xbox GIP metadata transfer acknowledgement.
 *
 * Matches report 4 and the active metadata progress through the shared acknowledgement decoder.
 *
 * @param[in,out] download Metadata download waiting for acknowledgement.
 * @param[in] acknowledgement Thirteen-byte acknowledgement packet.
 * @return True when every acknowledgement field matches current metadata progress.
 */
bool usb_xbox_gip_metadata_download_acknowledge(
    UsbXboxGipMetadataDownload *download,
    const uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE]) {
    return usb_feature_download_acknowledge(download, acknowledgement);
}
