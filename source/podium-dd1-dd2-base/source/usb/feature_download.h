#ifndef OPENTEC_BASE_USB_FEATURE_DOWNLOAD_H
#define OPENTEC_BASE_USB_FEATURE_DOWNLOAD_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Feature-download packet and acknowledgement sizes in bytes. */
enum {
    USB_FEATURE_DOWNLOAD_PACKET_SIZE = 64,          /**< Feature-download packet size. */
    USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE = 13, /**< Acknowledgement packet size. */
};

/** @brief Segmented feature-download progress and framing state. */
typedef struct {
    uint16_t total_length;      /**< Logical transfer length including an optional leading zero. */
    uint16_t offset;            /**< Logical bytes already emitted. */
    uint8_t report_id;          /**< Feature report identifier carried by packets. */
    uint8_t sequence;           /**< Sequence carried by every packet. */
    uint8_t continuation_count; /**< Continuation packets since the last acknowledgement. */
    bool leading_zero;          /**< True when a zero precedes the caller-owned source bytes. */
    bool awaiting_acknowledgement; /**< True while packet emission waits for acknowledgement. */
    bool complete;                 /**< True after the terminal packet has been emitted. */
} UsbFeatureDownload;

/**
 * @brief Initializes a segmented USB feature download.
 *
 * Selects report identifier, sequence, logical transfer length, and optional leading-zero framing,
 * then resets progress and acknowledgement cadence.
 *
 * @param[out] download Download state to initialize.
 * @param[in] report_id Feature report identifier.
 * @param[in] sequence Sequence carried by every transfer packet.
 * @param[in] total_length Logical transfer byte count including an optional leading zero.
 * @param[in] leading_zero Inserts a zero before caller-owned source bytes when true.
 */
void usb_feature_download_init(UsbFeatureDownload *download, uint8_t report_id, uint8_t sequence,
                               uint16_t total_length, bool leading_zero);

/**
 * @brief Builds the next segmented USB feature packet.
 *
 * Emits single, initial, continuation, final, or terminal packets according to transfer progress
 * and acknowledgement state.
 *
 * @param[in,out] download Active feature download.
 * @param[in] data Source bytes excluding the optional leading zero.
 * @param[out] packet Destination for the next feature packet.
 * @return Number of packet bytes produced, or zero while waiting, invalid, or complete.
 */
uint8_t usb_feature_download_next(UsbFeatureDownload *download, const uint8_t *data,
                                  uint8_t packet[USB_FEATURE_DOWNLOAD_PACKET_SIZE]);

/**
 * @brief Accepts a segmented feature-download acknowledgement.
 *
 * Matches status, response code, report identifier, transferred byte count, and remaining byte
 * count at the current acknowledgement boundary.
 *
 * @param[in,out] download Download waiting for acknowledgement.
 * @param[in] acknowledgement Thirteen-byte acknowledgement packet.
 * @return True when every acknowledgement field matches current transfer progress; otherwise false.
 */
bool usb_feature_download_acknowledge(
    UsbFeatureDownload *download,
    const uint8_t acknowledgement[USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE]);

#endif
