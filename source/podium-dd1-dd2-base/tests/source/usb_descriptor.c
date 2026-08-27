#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "usb/descriptor.h"

static void test_device_identity(void) {
    UsbDeviceIdentity identity = {
        .usb_version = 0x0200,
        .vendor_id = 0x0eb7,
        .product_id = 0x0004,
        .device_version = 0x0523,
        .control_packet_size = 64,
        .manufacturer_string = 1,
        .product_string = 3,
    };
    uint8_t descriptor[USB_DEVICE_DESCRIPTOR_SIZE];

    usb_device_descriptor_encode(&identity, descriptor);

    assert(descriptor[0] == USB_DEVICE_DESCRIPTOR_SIZE);
    assert(descriptor[1] == 1);
    assert(descriptor[2] == 0x00 && descriptor[3] == 0x02);
    assert(descriptor[7] == 64);
    assert(descriptor[8] == 0xb7 && descriptor[9] == 0x0e);
    assert(descriptor[10] == 0x04 && descriptor[11] == 0x00);
    assert(descriptor[12] == 0x23 && descriptor[13] == 0x05);
    assert(descriptor[14] == 1 && descriptor[15] == 3 && descriptor[16] == 0);
    assert(descriptor[17] == 1);
}

static void test_hid_configuration(void) {
    UsbHidConfiguration configuration = {
        .hid_version = 0x0111,
        .report_descriptor_size = 0x0128,
        .endpoint_packet_size = 64,
        .maximum_power_ma = 80,
        .country_code = 0x21,
        .input_endpoint = 0x81,
        .output_endpoint = 0x01,
        .poll_interval_ms = 1,
        .self_powered = true,
    };
    uint8_t descriptor[USB_HID_CONFIGURATION_DESCRIPTOR_SIZE];

    usb_hid_configuration_descriptor_encode(&configuration, descriptor);

    assert(descriptor[0] == 9 && descriptor[1] == 2);
    assert(descriptor[2] == USB_HID_CONFIGURATION_DESCRIPTOR_SIZE && descriptor[3] == 0);
    assert(descriptor[7] == 0xc0 && descriptor[8] == 40);
    assert(descriptor[9] == 9 && descriptor[10] == 4 && descriptor[13] == 2);
    assert(descriptor[14] == 3);
    assert(descriptor[18] == 9 && descriptor[19] == 0x21);
    assert(descriptor[20] == 0x11 && descriptor[21] == 0x01);
    assert(descriptor[22] == 0x21 && descriptor[23] == 1 && descriptor[24] == 0x22);
    assert(descriptor[25] == 0x28 && descriptor[26] == 0x01);
    assert(descriptor[27] == 7 && descriptor[29] == 0x81);
    assert(descriptor[31] == 64 && descriptor[33] == 1);
    assert(descriptor[34] == 7 && descriptor[36] == 0x01);
    assert(descriptor[38] == 64 && descriptor[40] == 1);
}

static void test_string_descriptors(void) {
    uint8_t descriptor[64];

    size_t length = usb_language_descriptor_encode(0x0409, descriptor, sizeof(descriptor));
    assert(length == 4);
    assert(descriptor[0] == 4 && descriptor[1] == 3);
    assert(descriptor[2] == 0x09 && descriptor[3] == 0x04);

    length = usb_string_descriptor_encode("Fanatec", descriptor, sizeof(descriptor));
    assert(length == 16);
    assert(descriptor[0] == 16 && descriptor[1] == 3);
    assert(descriptor[2] == 'F' && descriptor[3] == 0);
    assert(descriptor[14] == 'c' && descriptor[15] == 0);
}

static void test_rejects_short_buffers(void) {
    uint8_t descriptor[3];
    assert(usb_language_descriptor_encode(0x0409, descriptor, sizeof(descriptor)) == 0);
    assert(usb_string_descriptor_encode("a", descriptor, sizeof(descriptor)) == 0);
}

int main(void) {
    test_device_identity();
    test_hid_configuration();
    test_string_descriptors();
    test_rejects_short_buffers();
    return 0;
}
