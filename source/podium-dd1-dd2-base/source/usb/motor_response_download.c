#include "usb/motor_response_download.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_download.h"

bool usb_motor_response_download_init(UsbMotorResponseDownload *download, uint8_t sequence,
                                      uint16_t payload_length) {
    if (download == 0 || payload_length == UINT16_MAX) {
        return false;
    }
    usb_feature_download_init(download, USB_MOTOR_RESPONSE_REPORT_ID, sequence, payload_length + 1u,
                              true);
    return true;
}

uint8_t usb_motor_response_download_next(UsbMotorResponseDownload *download, const uint8_t *payload,
                                         uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]) {
    return usb_feature_download_next(download, payload, packet);
}

bool usb_motor_response_download_acknowledge(
    UsbMotorResponseDownload *download,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]) {
    return usb_feature_download_acknowledge(download, acknowledgement);
}
