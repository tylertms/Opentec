#ifndef OPENTEC_BASE_USB_MOTOR_RESPONSE_DOWNLOAD_H
#define OPENTEC_BASE_USB_MOTOR_RESPONSE_DOWNLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_download.h"

enum {
    USB_MOTOR_RESPONSE_REPORT_ID = 6,
    USB_MOTOR_RESPONSE_PACKET_SIZE = USB_FEATURE_DOWNLOAD_PACKET_SIZE,
    USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE = USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE,
};

typedef UsbFeatureDownload UsbMotorResponseDownload;

bool usb_motor_response_download_init(UsbMotorResponseDownload *download, uint8_t sequence,
                                      uint16_t payload_length);
uint8_t usb_motor_response_download_next(UsbMotorResponseDownload *download, const uint8_t *payload,
                                         uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]);
bool usb_motor_response_download_acknowledge(
    UsbMotorResponseDownload *download,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]);

#endif
