#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/feature_upload.h"

static void test_assembles_segmented_upload(void) {
    uint8_t source[131];
    uint8_t assembly[sizeof(source)];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {0};
    UsbFeatureUpload upload;
    for (uint8_t index = 0; index < sizeof(source); index++) {
        source[index] = index;
    }
    assert(usb_feature_upload_init(&upload, 6, assembly, sizeof(assembly)));

    packet[0] = 6;
    packet[1] = 0xf0;
    packet[2] = 7;
    packet[3] = 58;
    packet[4] = 0x83;
    packet[5] = 1;
    memcpy(packet + 6, source, 58);
    UsbFeatureUploadEvent event = usb_feature_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT);
    assert(upload.offset == 58 && upload.sequence == 7);

    packet[1] = 0xa0;
    packet[2] = 8;
    packet[3] = 0xba;
    packet[4] = 0;
    packet[5] = 58;
    memcpy(packet + 6, source + 58, 58);
    event = usb_feature_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_FEATURE_UPLOAD_WAITING);
    assert(upload.offset == 116 && upload.sequence == 8);

    packet[1] = 0xb0;
    packet[3] = 15;
    packet[4] = 0xf4;
    packet[5] = 0;
    memcpy(packet + 6, source + 116, 15);
    event = usb_feature_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT);
    assert(upload.offset == sizeof(source));

    packet[1] = 0xa0;
    packet[3] = 0;
    packet[4] = 0x83;
    packet[5] = 1;
    event = usb_feature_upload_accept(&upload, packet, sizeof(packet));
    assert(event.result == USB_FEATURE_UPLOAD_COMPLETE);
    assert(event.data == assembly && event.length == sizeof(source));
    assert(memcmp(assembly, source, sizeof(source)) == 0);
}

static void test_rejects_invalid_initial_packet(void) {
    uint8_t assembly[64];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0xf0, 1, 58, 0xba, 0};
    UsbFeatureUpload upload;
    assert(usb_feature_upload_init(&upload, 6, assembly, sizeof(assembly)));

    packet[0] = 5;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_INVALID);
    packet[0] = 6;
    packet[1] = 0xa0;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_INVALID);
    packet[1] = 0xf0;
    packet[5] = 1;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_INVALID);
}

static void test_rejects_invalid_final_and_terminal_packets(void) {
    uint8_t assembly[60];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0xf0, 1, 58, 0xbc, 0};
    UsbFeatureUpload upload;
    assert(usb_feature_upload_init(&upload, 6, assembly, sizeof(assembly)));
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT);

    packet[1] = 0xb0;
    packet[3] = 1;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_INVALID);
    packet[3] = 2;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT);
    packet[1] = 0xa0;
    packet[3] = 0;
    packet[4] = 0xbb;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_INVALID);
    packet[4] = 0xbc;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_COMPLETE);
}

static void test_rejects_invalid_progress(void) {
    uint8_t assembly[64];
    uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6};
    UsbFeatureUpload upload;
    assert(usb_feature_upload_init(&upload, 6, assembly, sizeof(assembly)));
    upload.active = true;
    upload.total_length = 1;
    upload.offset = 2;
    assert(usb_feature_upload_accept(&upload, packet, sizeof(packet)).result ==
           USB_FEATURE_UPLOAD_INVALID);
}

int main(void) {
    test_assembles_segmented_upload();
    test_rejects_invalid_initial_packet();
    test_rejects_invalid_final_and_terminal_packets();
    test_rejects_invalid_progress();
    return 0;
}
