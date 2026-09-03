#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "usb/motor_command_upload.h"

static void test_exposes_compact_command(void) {
    uint8_t assembly[128];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 0x2a, 12, 0, 0xc1, 0x12, 0x34};
    UsbMotorCommandUpload upload;
    bool initialized = usb_motor_command_upload_init(&upload, assembly, sizeof(assembly));
    assert(initialized);

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
    bool initialized = usb_motor_command_upload_init(&upload, assembly, sizeof(assembly));
    assert(initialized);

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
    bool initialized = usb_motor_command_upload_init(&upload, assembly, sizeof(assembly));
    assert(initialized);

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
    assert(event.segmented);
    assert(event.payload == assembly + 1 && event.payload_length == 122);
    assert(memcmp(event.payload, logical + 1, event.payload_length) == 0);
    assert(!upload.feature.active && !upload.feature.complete);
    usb_motor_command_upload_reset(&upload);
    assert(!upload.feature.active && !upload.feature.complete);
}

static void test_exposes_maximum_segmented_command(void) {
    static uint8_t logical[USB_MOTOR_COMMAND_UPLOAD_ASSEMBLY_SIZE];
    static uint8_t assembly[USB_MOTOR_COMMAND_UPLOAD_ASSEMBLY_SIZE];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {0};
    UsbMotorCommandUpload upload;
    const uint16_t fragment_length = 58;
    const uint16_t total_length = sizeof(logical);
    uint16_t offset = 0;

    for (uint16_t index = 0; index < total_length; index++) {
        logical[index] = (uint8_t)index;
    }
    assert(total_length == MOTOR_COMMAND_PACKET_MAX_PACKET_SIZE);
    bool initialized = usb_motor_command_upload_init(&upload, assembly, sizeof(assembly));
    assert(initialized);

    packet[0] = USB_MOTOR_COMMAND_REPORT_ID;
    packet[1] = 0xf0;
    packet[2] = 1;
    packet[3] = fragment_length;
    packet[4] = (uint8_t)(0x80 | (total_length & 0x7f));
    packet[5] = (uint8_t)(total_length >> 7);
    memcpy(packet + 6, logical, fragment_length);
    UsbMotorCommandUploadEvent event =
        usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT);
    offset += fragment_length;

    for (uint8_t sequence = 2; offset + fragment_length < total_length; sequence++) {
        packet[1] = 0xa0;
        packet[2] = sequence;
        packet[3] = 0xba;
        packet[4] = 0;
        packet[5] = 0;
        memcpy(packet + 6, logical + offset, fragment_length);
        event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
        assert(event.result == USB_MOTOR_COMMAND_UPLOAD_WAITING);
        offset += fragment_length;
    }

    packet[1] = 0xb0;
    packet[2]++;
    packet[3] = (uint8_t)(total_length - offset);
    packet[4] = 0;
    packet[5] = 0;
    memcpy(packet + 6, logical + offset, total_length - offset);
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT);

    packet[1] = 0xa0;
    packet[2]++;
    packet[3] = 0;
    packet[4] = (uint8_t)(0x80 | (total_length & 0x7f));
    packet[5] = (uint8_t)(total_length >> 7);
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_COMMAND);
    assert(event.segmented);
    assert(event.payload == assembly + 1);
    assert(event.payload_length ==
           MOTOR_COMMAND_PACKET_MAX_PACKET_SIZE - USB_MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE);
    assert(memcmp(event.payload, logical + 1, event.payload_length) == 0);
    assert(!upload.feature.active && !upload.feature.complete);
}

static void test_rejects_invalid_compact_length(void) {
    uint8_t assembly[64];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 1, 8};
    UsbMotorCommandUpload upload;
    bool initialized = usb_motor_command_upload_init(&upload, assembly, sizeof(assembly));
    assert(initialized);
    UsbMotorCommandUploadEvent event =
        usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_INVALID);
    packet[3] = 61;
    event = usb_motor_command_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_MOTOR_COMMAND_UPLOAD_INVALID);
}

int main(void) {
    test_exposes_compact_command();
    test_exposes_control_requests();
    test_exposes_segmented_command();
    test_exposes_maximum_segmented_command();
    test_rejects_invalid_compact_length();
    return 0;
}
