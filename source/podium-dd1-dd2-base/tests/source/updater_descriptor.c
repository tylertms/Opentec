#include "usb/updater_descriptor.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_identity(void) {
    UsbDeviceIdentity identity = usb_updater_device_identity();
    assert(identity.usb_version == 0x0200);
    assert(identity.device_class == 0x02);
    assert(identity.device_subclass == 0x00);
    assert(identity.device_protocol == 0x00);
    assert(identity.vendor_id == 0x0eb7);
    assert(identity.product_id == 0x0718);
    assert(identity.device_version == 0x0001);
    assert(identity.control_packet_size == 64);
    assert(identity.manufacturer_string == 1);
    assert(identity.product_string == 7);
    assert(identity.serial_string == 0);
    assert(strcmp(usb_updater_product_name(), "FANATEC EBLDC Updater") == 0);
}

static void test_configuration(void) {
    static const uint8_t expected[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE] = {
        0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0xc0, 0x32, 0x09, 0x04, 0x00, 0x00, 0x01,
        0x02, 0x02, 0x01, 0x00, 0x05, 0x24, 0x00, 0x10, 0x01, 0x04, 0x24, 0x02, 0x02, 0x05,
        0x24, 0x06, 0x00, 0x01, 0x05, 0x24, 0x01, 0x00, 0x01, 0x07, 0x05, 0x82, 0x03, 0x08,
        0x00, 0x02, 0x09, 0x04, 0x01, 0x00, 0x02, 0x0a, 0x00, 0x00, 0x00, 0x07, 0x05, 0x03,
        0x02, 0x40, 0x00, 0x00, 0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00,
    };
    uint8_t encoded[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE];
    usb_updater_configuration_descriptor_encode(encoded);
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

int main(void) {
    test_identity();
    test_configuration();
    return 0;
}
