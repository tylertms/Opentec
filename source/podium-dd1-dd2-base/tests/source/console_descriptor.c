#include "usb/console_descriptor.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t descriptor_hash(const uint8_t *data, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t index = 0; index < length; index++) {
        hash = (hash ^ data[index]) * 16777619u;
    }
    return hash;
}

static void test_xbox_gip_identity(void) {
    UsbDeviceIdentity identity = usb_xbox_gip_device_identity(0x0f50);
    assert(identity.usb_version == 0x0200);
    assert(identity.vendor_id == 0x0eb7 && identity.product_id == 0x0f50);
    assert(identity.device_version == 0x0523);
    assert(identity.device_class == 0xff && identity.device_subclass == 0xff);
    assert(identity.device_protocol == 0xff && identity.control_packet_size == 64);
    assert(identity.manufacturer_string == 1 && identity.product_string == 2);
    assert(identity.serial_string == 3);
    assert(strcmp(usb_xbox_gip_product_name(BOARD_VARIANT_DD1), "FANATEC Podium Wheel Base DD1") ==
           0);
    assert(strcmp(usb_xbox_gip_product_name(BOARD_VARIANT_DD2), "FANATEC Podium Wheel Base DD2") ==
           0);
    assert(strcmp(usb_xbox_gip_initial_serial(), "0000000000000000") == 0);
}

static void test_xbox_gip_configuration(void) {
    static const uint8_t expected[USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xe0, 0x28, 0x09, 0x04,
        0x00, 0x00, 0x02, 0xff, 0x47, 0xd0, 0x00, 0x07, 0x05, 0x01, 0x03,
        0x40, 0x00, 0x04, 0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x04,
    };
    uint8_t descriptor[sizeof(expected)];
    usb_xbox_gip_configuration_descriptor_encode(descriptor);
    assert(memcmp(descriptor, expected, sizeof(expected)) == 0);
}

static void test_playstation_identity(void) {
    UsbDeviceIdentity dd1 = usb_playstation_device_identity(BOARD_VARIANT_DD1);
    UsbDeviceIdentity dd2 = usb_playstation_device_identity(BOARD_VARIANT_DD2);
    assert(dd1.usb_version == 0x0200 && dd1.vendor_id == 0x0eb7);
    assert(dd1.product_id == 0x0e05 && dd2.product_id == 0x0e06);
    assert(dd1.device_version == 0x0523 && dd1.control_packet_size == 64);
    assert(dd1.manufacturer_string == 1 && dd1.product_string == 9);
    assert(strcmp(usb_playstation_product_name(BOARD_VARIANT_DD1),
                  "FANATEC Podium Wheel Base DD1 PlayStation 4") == 0);
    assert(strcmp(usb_playstation_product_name(BOARD_VARIANT_DD2),
                  "FANATEC Podium Wheel Base DD2 PlayStation 4") == 0);
}

static void test_playstation_configuration(void) {
    static const uint8_t expected[USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0xc0, 0x28, 0x09, 0x04, 0x00, 0x00, 0x02,
        0x03, 0x00, 0x00, 0x00, 0x09, 0x21, 0x11, 0x01, 0x21, 0x01, 0x22, 0xa0, 0x00, 0x07,
        0x05, 0x03, 0x03, 0x40, 0x00, 0x05, 0x07, 0x05, 0x84, 0x03, 0x40, 0x00, 0x05,
    };
    uint8_t descriptor[sizeof(expected)];
    usb_playstation_configuration_descriptor_encode(descriptor);
    assert(memcmp(descriptor, expected, sizeof(expected)) == 0);
}

static void test_playstation_report(void) {
    uint8_t descriptor[USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE];
    usb_playstation_report_descriptor_encode(descriptor);
    assert(descriptor_hash(descriptor, sizeof(descriptor)) == 0xd86d527fu);
    assert(descriptor[6] == 0x85 && descriptor[7] == 0x01);
    assert(descriptor[101] == 0x85 && descriptor[102] == 0x05);
    assert(descriptor[109] == 0x85 && descriptor[110] == 0x03);
    assert(descriptor[126] == 0x85 && descriptor[127] == 0xf0);
    assert(descriptor[157] == 0xb1 && descriptor[159] == 0xc0);
}

int main(void) {
    test_xbox_gip_identity();
    test_xbox_gip_configuration();
    test_playstation_identity();
    test_playstation_configuration();
    test_playstation_report();
    return 0;
}
