#ifndef OPENTEC_BASE_USB_FEATURE_UPLOAD_H
#define OPENTEC_BASE_USB_FEATURE_UPLOAD_H

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_FEATURE_UPLOAD_PACKET_SIZE = 64,
};

typedef enum {
    USB_FEATURE_UPLOAD_INVALID,
    USB_FEATURE_UPLOAD_WAITING,
    USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT,
    USB_FEATURE_UPLOAD_COMPLETE,
} UsbFeatureUploadResult;

typedef struct {
    UsbFeatureUploadResult result;
    const uint8_t *data;
    uint16_t length;
} UsbFeatureUploadEvent;

typedef struct {
    uint8_t *data;
    uint16_t capacity;
    uint16_t total_length;
    uint16_t offset;
    uint8_t report_id;
    uint8_t sequence;
    bool active;
    bool complete;
} UsbFeatureUpload;

bool usb_feature_upload_init(UsbFeatureUpload *upload, uint8_t report_id, uint8_t *data,
                             uint16_t capacity);
UsbFeatureUploadEvent
usb_feature_upload_accept(UsbFeatureUpload *upload,
                          const uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE], uint8_t length);

#endif
