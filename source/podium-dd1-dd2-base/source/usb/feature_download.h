#ifndef OPENTEC_BASE_USB_FEATURE_DOWNLOAD_H
#define OPENTEC_BASE_USB_FEATURE_DOWNLOAD_H

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_FEATURE_DOWNLOAD_PACKET_SIZE = 64,
    USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE = 13,
};

typedef struct {
    uint16_t total_length;
    uint16_t offset;
    uint8_t report_id;
    uint8_t sequence;
    uint8_t continuation_count;
    bool leading_zero;
    bool awaiting_acknowledgement;
    bool complete;
} UsbFeatureDownload;

void usb_feature_download_init(UsbFeatureDownload *download, uint8_t report_id, uint8_t sequence,
                               uint16_t total_length, bool leading_zero);
uint8_t usb_feature_download_next(UsbFeatureDownload *download, const uint8_t *data,
                                  uint8_t packet[USB_FEATURE_DOWNLOAD_PACKET_SIZE]);
bool usb_feature_download_acknowledge(
    UsbFeatureDownload *download,
    const uint8_t acknowledgement[USB_FEATURE_DOWNLOAD_ACKNOWLEDGEMENT_SIZE]);

#endif
