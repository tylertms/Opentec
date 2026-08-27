#include <assert.h>
#include <stdint.h>

#include "usb/control_request.h"

static UsbSetupPacket decode(const uint8_t data[USB_SETUP_PACKET_SIZE]) {
    UsbSetupPacket packet;
    assert(usb_setup_packet_decode(data, &packet));
    return packet;
}

static void test_decodes_setup_packet(void) {
    const uint8_t data[] = {0x80, 0x06, 0x00, 0x01, 0x02, 0x00, 0x12, 0x00};
    UsbSetupPacket packet = decode(data);

    assert(packet.request_type == 0x80);
    assert(packet.request == 6);
    assert(packet.value == 0x0100);
    assert(packet.index == 2);
    assert(packet.length == 18);
}

static void test_classifies_descriptor_request(void) {
    const uint8_t data[] = {0x80, 0x06, 0x03, 0x03, 0x09, 0x04, 0xff, 0x00};
    UsbSetupPacket packet = decode(data);
    UsbControlRequest request;

    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_GET_DESCRIPTOR);
    assert(request.descriptor_type == 3);
    assert(request.descriptor_index == 3);
    assert(request.index == 0x0409);
    assert(request.length == 255);
}

static void test_classifies_enumeration_state_changes(void) {
    const uint8_t address_data[] = {0x00, 0x05, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t configuration_data[] = {0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    UsbControlRequest request;
    UsbSetupPacket packet = decode(address_data);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_SET_ADDRESS);
    assert(request.value == 42);

    packet = decode(configuration_data);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_SET_CONFIGURATION);
    assert(request.value == 1);
}

static void test_classifies_hid_requests(void) {
    const uint8_t get_report_data[] = {0xa1, 0x01, 0x01, 0x01, 0x00, 0x00, 0x22, 0x00};
    const uint8_t set_idle_data[] = {0x21, 0x0a, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00};
    UsbControlRequest request;
    UsbSetupPacket packet = decode(get_report_data);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_HID_GET_REPORT);
    assert(request.value == 0x0101);

    packet = decode(set_idle_data);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_HID_SET_IDLE);
    assert(request.value == 0x0500);
}

static void test_rejects_invalid_requests(void) {
    const uint8_t invalid_address[] = {0x00, 0x05, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t invalid_protocol[] = {0x21, 0x0b, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    UsbControlRequest request;
    UsbSetupPacket packet = decode(invalid_address);
    assert(!usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_UNSUPPORTED);

    packet = decode(invalid_protocol);
    assert(!usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_UNSUPPORTED);
    assert(!usb_setup_packet_decode(0, &packet));
}

int main(void) {
    test_decodes_setup_packet();
    test_classifies_descriptor_request();
    test_classifies_enumeration_state_changes();
    test_classifies_hid_requests();
    test_rejects_invalid_requests();
    return 0;
}
