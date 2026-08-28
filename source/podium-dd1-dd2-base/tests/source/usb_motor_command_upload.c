#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/motor_command_upload.h"

static void test_exposes_compact_command(void) {
    uint8_t assembly[128];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 0x2a, 12, 0, 0xc1, 0x12, 0x34};
    UsbMotorCommandUpload upload;
    assert(usb_motor_command_upload_init(&upload, assembly, sizeof(assembly)));

    UsbMotorCommandUploadEvent event =
        usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_COMMAND);
    assert(event.payload == packet + 5 && event.payload_length == 3);
    assert(event.sequence == 0x2a);
    assert(event.acknowledgement_report_id == USB_MOTOR_COMMAND_COMPACT_ACKNOWLEDGEMENT_REPORT_ID);
}

static void test_exposes_control_requests(void) {
    uint8_t assembly[64];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 1, 9, 1, 1};
    UsbMotorCommandUpload upload;
    assert(usb_motor_command_upload_init(&upload, assembly, sizeof(assembly)));

    UsbMotorCommandUploadEvent event =
        usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_RESTART);
    packet[5] = 0;
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_RELEASE);
    packet[5] = 2;
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_INVALID);
}

static void test_exposes_segmented_command(void) {
    uint8_t logical[131] = {0};
    uint8_t assembly[sizeof(logical)];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {0};
    UsbMotorCommandUpload upload;
    for (uint8_t index = 1; index < sizeof(logical) - 8; index++) {
        logical[index] = index;
    }
    assert(usb_motor_command_upload_init(&upload, assembly, sizeof(assembly)));

    packet[0] = 6;
    packet[1] = 0xf0;
    packet[2] = 7;
    packet[3] = 58;
    packet[4] = 0x83;
    packet[5] = 1;
    memcpy(packet + 6, logical, 58);
    UsbMotorCommandUploadEvent event =
        usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT);
    assert(event.acknowledgement_report_id == USB_MOTOR_COMMAND_SEGMENT_ACKNOWLEDGEMENT_REPORT_ID);

    packet[1] = 0xa0;
    packet[3] = 0xba;
    packet[4] = 0;
    packet[5] = 58;
    memcpy(packet + 6, logical + 58, 58);
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_WAITING);

    packet[1] = 0xb0;
    packet[3] = 15;
    packet[4] = 0xf4;
    packet[5] = 0;
    memcpy(packet + 6, logical + 116, 15);
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT);

    packet[1] = 0xa0;
    packet[3] = 0;
    packet[4] = 0x83;
    packet[5] = 1;
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_COMMAND);
    assert(event.payload == assembly + 1 && event.payload_length == 122);
    assert(memcmp(event.payload, logical + 1, event.payload_length) == 0);
    assert(!upload.feature.active);
}

static void test_rejects_invalid_compact_length(void) {
    uint8_t assembly[64];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 1, 8};
    UsbMotorCommandUpload upload;
    assert(usb_motor_command_upload_init(&upload, assembly, sizeof(assembly)));
    assert(usb_motor_command_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_MOTOR_COMMAND_UPLOAD_INVALID);
    packet[3] = 61;
    assert(usb_motor_command_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_MOTOR_COMMAND_UPLOAD_INVALID);
}

int main(void) {
    test_exposes_compact_command();
    test_exposes_control_requests();
    test_exposes_segmented_command();
    test_rejects_invalid_compact_length();
    return 0;
}
