#include "usb/updater_protocol.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_decodes_bridge_request(void) {
    const uint8_t packet[] = {0x5a, 0xa7, 1, 2, 3};
    UsbUpdaterRequest request;
    assert(usb_updater_protocol_decode(packet, sizeof(packet), &request));
    assert(request.kind == USB_UPDATER_REQUEST_BRIDGE);
    assert(request.data == packet && request.length == sizeof(packet));
}

static void test_decodes_control_requests(void) {
    const uint8_t device_info[] = {0xf8, 0x01};
    const uint8_t reset[] = {0xf8, 0x09, 0x01, 0xfe};
    UsbUpdaterRequest request;
    assert(usb_updater_protocol_decode(device_info, sizeof(device_info), &request));
    assert(request.kind == USB_UPDATER_REQUEST_DEVICE_INFO);
    assert(usb_updater_protocol_decode(reset, sizeof(reset), &request));
    assert(request.kind == USB_UPDATER_REQUEST_RESET);
}

static void test_rejects_unsupported_packets(void) {
    uint8_t packet[64] = {0};
    UsbUpdaterRequest request;
    assert(!usb_updater_protocol_decode(NULL, 1, &request));
    assert(!usb_updater_protocol_decode(packet, 0, &request));
    assert(!usb_updater_protocol_decode(packet, sizeof(packet), &request));
    assert(!usb_updater_protocol_decode(packet, 1, NULL));
    packet[0] = 0x5a;
    assert(!usb_updater_protocol_decode(packet, 1, &request));
    packet[0] = 0xf8;
    assert(!usb_updater_protocol_decode(packet, 1, &request));
    packet[1] = 0x09;
    packet[2] = 0x01;
    packet[3] = 0xfd;
    assert(!usb_updater_protocol_decode(packet, 4, &request));
}

static void test_encodes_device_information(void) {
    const uint8_t identity[] = {'d', 'd', '1', '0'};
    const uint8_t expected[] = {0xf8, 0x01, 'd', 'd', '1', '0'};
    uint8_t response[USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE] = {0};
    usb_updater_protocol_encode_device_info(identity, response);
    assert(memcmp(response, expected, sizeof(expected)) == 0);
    usb_updater_protocol_encode_device_info(NULL, response);
    usb_updater_protocol_encode_device_info(identity, NULL);
}

int main(void) {
    test_decodes_bridge_request();
    test_decodes_control_requests();
    test_rejects_unsupported_packets();
    test_encodes_device_information();
    return 0;
}
