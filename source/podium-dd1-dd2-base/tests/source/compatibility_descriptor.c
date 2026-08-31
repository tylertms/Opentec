#include "usb/compatibility_descriptor.h"

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

static void test_profile(UsbInputReportMode mode, uint32_t expected_device_hash,
                         uint32_t expected_configuration_hash) {
    UsbDeviceIdentity identity;
    UsbHidConfiguration configuration;
    uint8_t device[USB_DEVICE_DESCRIPTOR_SIZE];
    uint8_t descriptor[USB_HID_CONFIGURATION_DESCRIPTOR_SIZE];

    assert(usb_compatibility_descriptor_profile(mode, &identity, &configuration));
    usb_device_descriptor_encode(&identity, device);
    usb_hid_configuration_descriptor_encode(&configuration, descriptor);
    assert(fnv1a(device, sizeof(device)) == expected_device_hash);
    assert(fnv1a(descriptor, sizeof(descriptor)) == expected_configuration_hash);
}

static void test_profiles(void) {
    test_profile(USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY, 0x8d2da375u, 0xfac17d26u);
    test_profile(USB_INPUT_REPORT_MODE_DRIVING_FORCE_EX, 0xd6c5eafdu, 0xab5adfa7u);
    test_profile(USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO, 0x90a47101u, 0xd8b86e06u);
    test_profile(USB_INPUT_REPORT_MODE_G27, 0xf2a1bfc1u, 0x63115ba5u);
}

static void test_validation(void) {
    UsbDeviceIdentity identity;
    UsbHidConfiguration configuration;

    assert(!usb_compatibility_descriptor_profile(USB_INPUT_REPORT_MODE_FANATEC, &identity,
                                                 &configuration));
    assert(!usb_compatibility_descriptor_profile(USB_INPUT_REPORT_MODE_G27, NULL, &configuration));
    assert(!usb_compatibility_descriptor_profile(USB_INPUT_REPORT_MODE_G27, &identity, NULL));
}

int main(void) {
    test_profiles();
    test_validation();
    return 0;
}
