#include "usb/motor_command_upload.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/feature_upload.h"

enum {
    MOTOR_COMMAND_UPLOAD_SEGMENTED = 0xf0,
    MOTOR_COMMAND_UPLOAD_COMPACT_FLAG = 0x10,
    MOTOR_COMMAND_UPLOAD_LENGTH_OFFSET = 3,
    MOTOR_COMMAND_UPLOAD_CONTROL_OFFSET = 4,
    MOTOR_COMMAND_UPLOAD_PAYLOAD_OFFSET = 5,
    MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE = 9,
    MOTOR_COMMAND_UPLOAD_COMPACT_CAPACITY = USB_FEATURE_UPLOAD_PACKET_SIZE - 4,
};

/**
 * @brief Initializes motor-command upload state.
 *
 * Attaches the caller-owned segmented assembly buffer and selects feature report 6.
 *
 * @param[out] upload Motor-command upload state to initialize.
 * @param[out] assembly Caller-owned segmented command assembly buffer.
 * @param[in] assembly_capacity Available assembly byte count.
 * @return True when the state and assembly storage are usable.
 */
bool usb_motor_command_upload_init(UsbMotorCommandUpload *upload, uint8_t *assembly,
                                   uint16_t assembly_capacity) {
    return upload != 0 && usb_feature_upload_init(&upload->feature, USB_MOTOR_COMMAND_REPORT_ID,
                                                  assembly, assembly_capacity);
}

/**
 * @brief Accepts one USB motor-command feature packet.
 *
 * Recognizes restart and release controls, exposes compact application payloads after their leading
 * status byte, and maps segmented upload progress into report 0xFD acknowledgement requests and a
 * completed application payload.
 *
 * @param[in,out] upload Active motor-command upload state.
 * @param[in] packet Sixty-four-byte feature report 6 packet.
 * @param[in] length Received packet byte count.
 * @return Invalid input, upload progress, acknowledgement request, control, or completed command.
 */
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
                   feature.length >= MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE) {
            event.result = USB_MOTOR_COMMAND_UPLOAD_COMMAND;
            event.payload = feature.data + 1;
            event.payload_length = feature.length - MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE;
            uint8_t *assembly = upload->feature.data;
            uint16_t capacity = upload->feature.capacity;
            usb_feature_upload_init(&upload->feature, USB_MOTOR_COMMAND_REPORT_ID, assembly,
                                    capacity);
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
    if (wrapped_length < MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE ||
        wrapped_length > MOTOR_COMMAND_UPLOAD_COMPACT_CAPACITY) {
        return event;
    }
    event.result = USB_MOTOR_COMMAND_UPLOAD_COMMAND;
    event.payload = &packet[MOTOR_COMMAND_UPLOAD_PAYLOAD_OFFSET];
    event.payload_length = wrapped_length - MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE;
    if ((packet[1] & MOTOR_COMMAND_UPLOAD_COMPACT_FLAG) != 0) {
        event.acknowledgement_report_id = USB_MOTOR_COMMAND_COMPACT_ACKNOWLEDGEMENT_REPORT_ID;
    }
    return event;
}
