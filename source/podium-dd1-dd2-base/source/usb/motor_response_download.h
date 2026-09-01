#ifndef OPENTEC_BASE_USB_MOTOR_RESPONSE_DOWNLOAD_H
#define OPENTEC_BASE_USB_MOTOR_RESPONSE_DOWNLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_download.h"

/** @brief Packet sizes and report identifier for motor responses. */
enum {
    USB_MOTOR_RESPONSE_REPORT_ID = 6, /**< Motor-response feature report identifier. */
    USB_MOTOR_RESPONSE_PACKET_SIZE =
        USB_FEATURE_DOWNLOAD_PACKET_SIZE, /**< Response packet size in bytes. */
    USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE =
        USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE, /**< Response acknowledgement size in bytes. */
};

/** @brief Feature-download state used to transmit a motor response. */
typedef UsbFeatureDownload UsbMotorResponseDownload;

/**
 * @brief Initializes a motor-response download.
 *
 * Configures feature report USB_MOTOR_RESPONSE_REPORT_ID and reserves one leading zero status byte
 * before the caller's response payload.
 *
 * @param[out] download Response download state to initialize.
 * @param[in] sequence Sequence carried by every response packet.
 * @param[in] payload_length Number of response payload bytes, excluding the status byte.
 * @return True when download is non-null and payload_length leaves room for the status byte;
 * otherwise false.
 */
bool usb_motor_response_download_init(UsbMotorResponseDownload *download, uint8_t sequence,
                                      uint16_t payload_length);

/**
 * @brief Builds the next motor-response packet.
 *
 * Encodes the next compact, segmented, or terminal packet and advances the download state when a
 * packet is produced.
 *
 * @param[in,out] download Active response download state.
 * @param[in] payload Response payload bytes, excluding the leading status byte.
 * @param[out] packet Buffer for USB_MOTOR_RESPONSE_PACKET_SIZE response bytes.
 * @return Number of packet bytes produced when a packet is ready; otherwise zero while awaiting an
 * acknowledgement, after completion, or when required pointers are invalid.
 */
uint8_t usb_motor_response_download_next(UsbMotorResponseDownload *download, const uint8_t *payload,
                                         uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]);

/**
 * @brief Accepts a motor-response acknowledgement.
 *
 * Validates the acknowledgement against the active report identifier and current transfer progress,
 * then allows the next response packet to be produced.
 *
 * @param[in,out] download Response download state awaiting an acknowledgement.
 * @param[in] acknowledgement Received acknowledgement packet.
 * @return True when the acknowledgement matches the current transfer boundary; otherwise false.
 */
bool usb_motor_response_download_acknowledge(
    UsbMotorResponseDownload *download,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]);

#endif
