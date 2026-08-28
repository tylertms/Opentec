#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/motor_response_download.h"

static void build_acknowledgement(uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE],
                                  uint16_t transferred, uint16_t remaining) {
    memset(acknowledgement, 0, USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE);
    acknowledgement[0] = 1;
    acknowledgement[5] = USB_MOTOR_RESPONSE_REPORT_ID;
    acknowledgement[7] = (uint8_t)transferred;
    acknowledgement[8] = (uint8_t)(transferred >> 8);
    acknowledgement[11] = (uint8_t)remaining;
    acknowledgement[12] = (uint8_t)(remaining >> 8);
}

static void test_downloads_compact_response(void) {
    static const uint8_t payload[] = {0xc1, 0x12, 0x34};
    static const uint8_t expected[] = {6, 0x30, 0x2a, 4, 0, 0xc1, 0x12, 0x34};
    static const uint8_t terminal[] = {6, 0xa0, 0x2a, 0, 4, 0};
    uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE];
    uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE];
    UsbMotorResponseDownload download;

    assert(usb_motor_response_download_init(&download, 0x2a, sizeof(payload)));
    assert(usb_motor_response_download_next(&download, payload, packet) == sizeof(expected));
    assert(memcmp(packet, expected, sizeof(expected)) == 0);
    assert(download.awaiting_acknowledgement);

    build_acknowledgement(acknowledgement, 4, 0);
    assert(usb_motor_response_download_acknowledge(&download, acknowledgement));
    assert(usb_motor_response_download_next(&download, payload, packet) == sizeof(terminal));
    assert(memcmp(packet, terminal, sizeof(terminal)) == 0);
    assert(download.complete);
}

static void test_downloads_segmented_response(void) {
    uint8_t payload[130];
    uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE];
    uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE];
    UsbMotorResponseDownload download;
    for (uint8_t index = 0; index < sizeof(payload); index++) {
        payload[index] = index;
    }

    assert(usb_motor_response_download_init(&download, 7, sizeof(payload)));
    assert(usb_motor_response_download_next(&download, payload, packet) == sizeof(packet));
    assert(packet[0] == 6 && packet[1] == 0xf0 && packet[2] == 7);
    assert(packet[3] == 0x3a && packet[4] == 0x83 && packet[5] == 1);
    assert(packet[6] == 0);
    assert(memcmp(&packet[7], payload, 57) == 0);

    build_acknowledgement(acknowledgement, 58, 73);
    assert(usb_motor_response_download_acknowledge(&download, acknowledgement));
    assert(usb_motor_response_download_next(&download, payload, packet) == sizeof(packet));
    assert(packet[1] == 0xa0 && packet[3] == 0xba && packet[4] == 0 && packet[5] == 58);
    assert(memcmp(&packet[6], payload + 57, 58) == 0);

    assert(usb_motor_response_download_next(&download, payload, packet) == 21);
    assert(packet[1] == 0xb0 && packet[3] == 15 && packet[4] == 0xf4 && packet[5] == 0);
    assert(memcmp(&packet[6], payload + 115, 15) == 0);
    build_acknowledgement(acknowledgement, 131, 0);
    assert(usb_motor_response_download_acknowledge(&download, acknowledgement));
    assert(usb_motor_response_download_next(&download, payload, packet) == 6);
    assert(packet[1] == 0xa0 && packet[3] == 0 && packet[4] == 0x83 && packet[5] == 1);
}

static void test_rejects_mismatched_acknowledgement(void) {
    static const uint8_t payload[] = {0xc0};
    uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE];
    uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE];
    UsbMotorResponseDownload download;

    assert(usb_motor_response_download_init(&download, 1, sizeof(payload)));
    assert(usb_motor_response_download_next(&download, payload, packet) != 0);
    build_acknowledgement(acknowledgement, 2, 0);
    acknowledgement[5] = 4;
    assert(!usb_motor_response_download_acknowledge(&download, acknowledgement));
    assert(download.awaiting_acknowledgement);
}

static void test_rejects_invalid_progress(void) {
    static const uint8_t payload[] = {0xc0};
    uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE] = {0};
    uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE];
    UsbMotorResponseDownload download;

    assert(usb_motor_response_download_init(&download, 1, sizeof(payload)));
    download.offset = download.total_length + 1;
    download.awaiting_acknowledgement = true;
    assert(usb_motor_response_download_next(&download, payload, packet) == 0);
    assert(!usb_motor_response_download_acknowledge(&download, acknowledgement));
}

int main(void) {
    test_downloads_compact_response();
    test_downloads_segmented_response();
    test_rejects_mismatched_acknowledgement();
    test_rejects_invalid_progress();
    return 0;
}
