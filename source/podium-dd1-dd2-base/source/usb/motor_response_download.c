#include "usb/motor_response_download.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_download.h"

/**
 * @brief Initializes a motor-command response download.
 *
 * Configures feature report 6 to prepend its zero status byte before the complete logical motor
 * application payload.
 *
 * @param[out] download Motor-response download state to initialize.
 * @param[in] sequence Sequence carried by every response packet.
 * @param[in] payload_length Motor application payload byte count.
 * @return True when the payload length can include the leading status byte.
 */
bool usb_motor_response_download_init(UsbMotorResponseDownload *download, uint8_t sequence,
                                      uint16_t payload_length) {
    if (download == 0 || payload_length == UINT16_MAX) {
        return false;
    }
    usb_feature_download_init(download, USB_MOTOR_RESPONSE_REPORT_ID, sequence, payload_length + 1u,
                              true);
    return true;
}

/**
 * @brief Builds the next motor-command response packet.
 *
 * Delegates compact and segmented response framing to the shared feature-download state machine.
 *
 * @param[in,out] download Active motor-response download.
 * @param[in] payload Complete logical motor application payload.
 * @param[out] packet Destination for the next response packet.
 * @return Number of packet bytes produced, or zero while waiting or after completion.
 */
uint8_t usb_motor_response_download_next(UsbMotorResponseDownload *download, const uint8_t *payload,
                                         uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]) {
    return usb_feature_download_next(download, payload, packet);
}

/**
 * @brief Accepts a motor-command response acknowledgement.
 *
 * Matches report 6 and the active download progress through the shared acknowledgement decoder.
 *
 * @param[in,out] download Motor response waiting for acknowledgement.
 * @param[in] acknowledgement Thirteen-byte acknowledgement packet.
 * @return True when the acknowledgement matches current response progress.
 */
bool usb_motor_response_download_acknowledge(
    UsbMotorResponseDownload *download,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]) {
    return usb_feature_download_acknowledge(download, acknowledgement);
}
