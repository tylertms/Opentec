#include "usb/motor_command_upload.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_upload.h"

/** @brief Internal fields and markers in motor-command upload packets. */
enum {
    MOTOR_COMMAND_UPLOAD_SEGMENTED = 0xf0,    /**< Segmented-upload packet marker. */
    MOTOR_COMMAND_UPLOAD_COMPACT_FLAG = 0x10, /**< Compact-upload acknowledgement flag. */
    MOTOR_COMMAND_UPLOAD_LENGTH_OFFSET = 3,   /**< Compact wrapper length byte offset. */
    MOTOR_COMMAND_UPLOAD_CONTROL_OFFSET = 4,  /**< Compact control byte offset. */
    MOTOR_COMMAND_UPLOAD_PAYLOAD_OFFSET = 5, /**< Compact payload byte offset. */
    MOTOR_COMMAND_UPLOAD_COMPACT_CAPACITY =
        USB_FEATURE_UPLOAD_PACKET_SIZE - 4, /**< Maximum compact wrapper length. */
};

bool usb_motor_command_upload_init(UsbMotorCommandUpload *upload, uint8_t *assembly,
                                   uint16_t assembly_capacity) {
    return upload != 0 && usb_feature_upload_init(&upload->feature, USB_MOTOR_COMMAND_REPORT_ID,
                                                  assembly, assembly_capacity);
}

void usb_motor_command_upload_reset(UsbMotorCommandUpload *upload) {
    if (upload == 0) {
        return;
    }
    upload->feature.active = false;
    upload->feature.complete = false;
    upload->feature.total_length = 0;
    upload->feature.offset = 0;
    upload->feature.sequence = 0;
}

UsbMotorCommandUploadEvent
usb_motor_command_upload_accept(UsbMotorCommandUpload *upload,
                                const uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE],
                                uint8_t length) {
    UsbMotorCommandUploadEvent event = {.result = USB_MOTOR_COMMAND_UPLOAD_INVALID};
    if (upload == 0 || packet == 0 || length != USB_FEATURE_UPLOAD_PACKET_SIZE ||
        packet[0] != USB_MOTOR_COMMAND_REPORT_ID) {
        return event;
    }
    event.sequence = packet[2];

    if (packet[1] == MOTOR_COMMAND_UPLOAD_SEGMENTED || upload->feature.active) {
        UsbFeatureUploadEvent feature = usb_feature_upload_accept(&upload->feature, packet, length);
        if (feature.result == USB_FEATURE_UPLOAD_WAITING) {
            event.result = USB_MOTOR_COMMAND_UPLOAD_WAITING;
        } else if (feature.result == USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT) {
            event.result = USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT;
            event.acknowledgement_report_id = USB_MOTOR_COMMAND_SEGMENT_ACKNOWLEDGEMENT_REPORT_ID;
        } else if (feature.result == USB_FEATURE_UPLOAD_COMPLETE &&
                   feature.length >= USB_MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE) {
            event.result = USB_MOTOR_COMMAND_UPLOAD_COMMAND;
            event.payload = feature.data + 1;
            event.payload_length = feature.length - USB_MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE;
            event.segmented = true;
            usb_motor_command_upload_reset(upload);
        }
        return event;
    }

    if (packet[MOTOR_COMMAND_UPLOAD_CONTROL_OFFSET] == 1) {
        if (packet[MOTOR_COMMAND_UPLOAD_PAYLOAD_OFFSET] == 1) {
            event.result = USB_MOTOR_COMMAND_UPLOAD_RESTART;
        } else if (packet[MOTOR_COMMAND_UPLOAD_PAYLOAD_OFFSET] == 0) {
            event.result = USB_MOTOR_COMMAND_UPLOAD_RELEASE;
        }
        return event;
    }

    uint8_t wrapped_length = packet[MOTOR_COMMAND_UPLOAD_LENGTH_OFFSET];
    if (wrapped_length < USB_MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE ||
        wrapped_length > MOTOR_COMMAND_UPLOAD_COMPACT_CAPACITY) {
        return event;
    }
    event.result = USB_MOTOR_COMMAND_UPLOAD_COMMAND;
    event.payload = &packet[MOTOR_COMMAND_UPLOAD_PAYLOAD_OFFSET];
    event.payload_length = wrapped_length - USB_MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE;
    if ((packet[1] & MOTOR_COMMAND_UPLOAD_COMPACT_FLAG) != 0) {
        event.acknowledgement_report_id = USB_MOTOR_COMMAND_COMPACT_ACKNOWLEDGEMENT_REPORT_ID;
    }
    return event;
}
