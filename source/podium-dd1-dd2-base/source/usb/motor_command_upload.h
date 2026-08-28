#ifndef OPENTEC_BASE_USB_MOTOR_COMMAND_UPLOAD_H
#define OPENTEC_BASE_USB_MOTOR_COMMAND_UPLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_upload.h"

enum {
    USB_MOTOR_COMMAND_REPORT_ID = 6,
    USB_MOTOR_COMMAND_SEGMENT_ACKNOWLEDGEMENT_REPORT_ID = 0xfd,
    USB_MOTOR_COMMAND_COMPACT_ACKNOWLEDGEMENT_REPORT_ID = 0xfe,
};

typedef enum {
    USB_MOTOR_COMMAND_UPLOAD_INVALID,
    USB_MOTOR_COMMAND_UPLOAD_WAITING,
    USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT,
    USB_MOTOR_COMMAND_UPLOAD_COMMAND,
    USB_MOTOR_COMMAND_UPLOAD_RESTART,
    USB_MOTOR_COMMAND_UPLOAD_RELEASE,
} UsbMotorCommandUploadResult;

typedef struct {
    UsbMotorCommandUploadResult result;
    const uint8_t *payload;
    uint16_t payload_length;
    uint8_t sequence;
    uint8_t acknowledgement_report_id;
} UsbMotorCommandUploadEvent;

typedef struct {
    UsbFeatureUpload feature;
} UsbMotorCommandUpload;

bool usb_motor_command_upload_init(UsbMotorCommandUpload *upload, uint8_t *assembly,
                                   uint16_t assembly_capacity);
UsbMotorCommandUploadEvent
usb_motor_command_upload_accept(UsbMotorCommandUpload *upload,
                                const uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE],
                                uint8_t length);

#endif
