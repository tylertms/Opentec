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

static void test_classifies_both_interface_states(void) {
    const uint8_t get_interface[] = {0x81, 0x0a, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00};
    const uint8_t set_interface[] = {0x01, 0x0b, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00};
    UsbControlRequest request;

    UsbSetupPacket packet = decode(get_interface);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_GET_INTERFACE && request.index == 1);

    packet = decode(set_interface);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_SET_INTERFACE);
    assert(request.index == 1 && request.value == 7);

    packet.index = 2;
    assert(!usb_control_request_classify(&packet, &request));
}

static void test_classifies_remote_wakeup_features(void) {
    const uint8_t set_feature[] = {0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t clear_feature[] = {0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    UsbControlRequest request;

    UsbSetupPacket packet = decode(set_feature);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_SET_FEATURE);

    packet = decode(clear_feature);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_CLEAR_FEATURE);

    packet.value = 0;
    assert(!usb_control_request_classify(&packet, &request));
    packet.value = 1;
    packet.index = 1;
    assert(!usb_control_request_classify(&packet, &request));
}

static void test_classifies_endpoint_halt_features(void) {
    const uint8_t set_feature[] = {0x02, 0x03, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00};
    const uint8_t clear_feature[] = {0x02, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    UsbControlRequest request;

    UsbSetupPacket packet = decode(set_feature);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_SET_FEATURE);
    assert(request.recipient == 2 && request.index == 0x81);

    packet = decode(clear_feature);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_CLEAR_FEATURE);
    assert(request.index == 4);

    packet.index = 0;
    assert(!usb_control_request_classify(&packet, &request));
    packet.index = 5;
    assert(!usb_control_request_classify(&packet, &request));
    packet.index = 0x41;
    assert(!usb_control_request_classify(&packet, &request));
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

static void test_classifies_cdc_requests(void) {
    const uint8_t set_line_coding[] = {0x21, 0x20, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};
    const uint8_t get_line_coding[] = {0xa1, 0x21, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};
    const uint8_t set_control_lines[] = {0x21, 0x22, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
    UsbControlRequest request;

    UsbSetupPacket packet = decode(set_line_coding);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_CDC_SET_LINE_CODING);

    packet = decode(get_line_coding);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_CDC_GET_LINE_CODING);

    packet = decode(set_control_lines);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_CDC_SET_CONTROL_LINE_STATE);
    assert(request.value == 3);
}

static void test_classifies_xbox_security_request(void) {
    const uint8_t data[] = {0xc0, 0x90, 0x00, 0x00, 0x04, 0x00, 0x28, 0x00};
    UsbControlRequest request;
    UsbSetupPacket packet = decode(data);
    assert(usb_control_request_classify(&packet, &request));
    assert(request.kind == USB_CONTROL_XBOX_SECURITY_DESCRIPTOR);
    assert(request.index == 4 && request.length == 40);

    packet.request_type = 0x40;
    assert(!usb_control_request_classify(&packet, &request));
    packet.request_type = 0xc0;
    packet.index = 3;
    assert(!usb_control_request_classify(&packet, &request));
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
    test_classifies_both_interface_states();
    test_classifies_remote_wakeup_features();
    test_classifies_endpoint_halt_features();
    test_classifies_hid_requests();
    test_classifies_cdc_requests();
    test_classifies_xbox_security_request();
    test_rejects_invalid_requests();
    return 0;
}
