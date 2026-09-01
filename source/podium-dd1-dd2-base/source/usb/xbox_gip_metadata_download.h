#ifndef OPENTEC_BASE_USB_XBOX_GIP_METADATA_DOWNLOAD_H
#define OPENTEC_BASE_USB_XBOX_GIP_METADATA_DOWNLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_download.h"
#include "usb/xbox_gip_metadata.h"

/** @brief Xbox GIP metadata-download framing constants. */
enum {
    USB_XBOX_GIP_METADATA_REPORT_ID =
        4, /**< Feature report identifier used for metadata packets. */
    USB_XBOX_GIP_METADATA_PACKET_SIZE =
        USB_FEATURE_DOWNLOAD_PACKET_SIZE, /**< Number of bytes in each metadata packet. */
    USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE =
        USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE, /**< Number of bytes in a metadata
                                                      acknowledgement. */
};

/** @brief Feature-download state specialized for Xbox GIP metadata. */
typedef UsbFeatureDownload UsbXboxGipMetadataDownload;

/**
 * @brief Starts an Xbox GIP metadata download.
 *
 * Configures feature report 4 to transfer the complete metadata document without a synthetic
 * leading byte.
 *
 * @param[out] download Download state to initialize.
 * @param[in] sequence Sequence value carried by the metadata packets.
 */
void usb_xbox_gip_metadata_download_init(UsbXboxGipMetadataDownload *download, uint8_t sequence);

/**
 * @brief Builds the next Xbox GIP metadata packet.
 *
 * Delegates segmented framing, acknowledgement boundaries, and terminal framing to the shared
 * feature-download state machine.
 *
 * @param[in,out] download Active metadata download state.
 * @param[in] metadata Encoded metadata document.
 * @param[out] packet Destination for the next metadata packet.
 * @return Number of packet bytes produced, or zero while waiting or after completion.
 */
uint8_t usb_xbox_gip_metadata_download_next(UsbXboxGipMetadataDownload *download,
                                            const uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE],
                                            uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE]);

/**
 * @brief Accepts an Xbox GIP metadata transfer acknowledgement.
 *
 * Matches report 4 and the active metadata progress through the shared acknowledgement decoder.
 *
 * @param[in,out] download Metadata download waiting for acknowledgement.
 * @param[in] acknowledgement Thirteen-byte acknowledgement packet.
 * @return `true` when every acknowledgement field matches current metadata progress; otherwise
 * `false`.
 */
bool usb_xbox_gip_metadata_download_acknowledge(
    UsbXboxGipMetadataDownload *download,
    const uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE]);

#endif
