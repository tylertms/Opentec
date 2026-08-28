#include "usb/compatibility_report_descriptor.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t fnv1a(const uint8_t *data, size_t length) {
    uint32_t hash = 0x811c9dc5u;
    for (size_t index = 0; index < length; index++) {
        hash = (hash ^ data[index]) * 0x01000193u;
    }
    return hash;
}

static void test_descriptor(UsbInputReportMode mode, size_t expected_size, uint32_t expected_hash) {
    uint8_t descriptor[USB_COMPATIBILITY_REPORT_DESCRIPTOR_MAX_SIZE];
    size_t size = usb_compatibility_report_descriptor_encode(mode, descriptor, sizeof(descriptor));

    assert(size == expected_size);
    assert(fnv1a(descriptor, size) == expected_hash);
}

static void test_descriptors(void) {
    test_descriptor(USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY,
                    USB_FANATEC_COMPATIBILITY_REPORT_DESCRIPTOR_SIZE, 0x307067a3u);
    test_descriptor(USB_INPUT_REPORT_MODE_DRIVING_FORCE_EX,
                    USB_DRIVING_FORCE_EX_REPORT_DESCRIPTOR_SIZE, 0x3aea0020u);
    test_descriptor(USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO,
                    USB_DRIVING_FORCE_PRO_REPORT_DESCRIPTOR_SIZE, 0x3620b1c2u);
    test_descriptor(USB_INPUT_REPORT_MODE_G27, USB_G27_REPORT_DESCRIPTOR_SIZE, 0x9b592a37u);
}

static void test_validation(void) {
    uint8_t descriptor[USB_COMPATIBILITY_REPORT_DESCRIPTOR_MAX_SIZE];

    assert(usb_compatibility_report_descriptor_encode(USB_INPUT_REPORT_MODE_FANATEC, descriptor,
                                                      sizeof(descriptor)) == 0);
    assert(usb_compatibility_report_descriptor_encode(USB_INPUT_REPORT_MODE_G27, NULL,
                                                      sizeof(descriptor)) == 0);
    assert(usb_compatibility_report_descriptor_encode(USB_INPUT_REPORT_MODE_G27, descriptor,
                                                      USB_G27_REPORT_DESCRIPTOR_SIZE - 1) == 0);
}

int main(void) {
    test_descriptors();
    test_validation();
    return 0;
}
