#include "usb/updater_identity.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static void assert_identity(UsbRuntimeMode runtime_mode, uint8_t wheel_mode,
                            uint8_t response_selector, bool adapter_connected,
                            const char expected[4]) {
    UsbUpdaterIdentityInput input = {
        .runtime_mode = runtime_mode,
        .wheel_mode = wheel_mode,
        .response_selector = response_selector,
        .adapter_connected = adapter_connected,
    };
    uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE];
    usb_updater_identity_select(&input, identity);
    assert(memcmp(identity, expected, sizeof(identity)) == 0);
}

static void test_selects_runtime_identity(void) {
    assert_identity(USB_RUNTIME_MODE_NORMAL, 0, USB_UPDATER_IDENTITY_AUTOMATIC, false, "FFFF");
    assert_identity(USB_RUNTIME_MODE_AUXILIARY, 0, USB_UPDATER_IDENTITY_AUTOMATIC, false, "kcfg");
    assert_identity(USB_RUNTIME_MODE_AUXILIARY_RECOVERY, 0, USB_UPDATER_IDENTITY_AUTOMATIC, false,
                    "dd10");
    assert_identity(USB_RUNTIME_MODE_STATUS_BRIDGE, 0, USB_UPDATER_IDENTITY_AUTOMATIC, false,
                    "pdqr");
}

static void test_selects_usb_wheel_identity(void) {
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 9, USB_UPDATER_IDENTITY_AUTOMATIC, false, "r650");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 0x10, USB_UPDATER_IDENTITY_AUTOMATIC, false,
                    "wgts");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 0x1d, USB_UPDATER_IDENTITY_AUTOMATIC, false,
                    "wgt3");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 4, USB_UPDATER_IDENTITY_AUTOMATIC, false, "wmcl");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 6, USB_UPDATER_IDENTITY_AUTOMATIC, true, "phub");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 0, USB_UPDATER_IDENTITY_AUTOMATIC, false, "rfor");
}

static void test_selects_protocol_and_explicit_identity(void) {
    assert_identity(USB_RUNTIME_MODE_PROTOCOL_BRIDGE, 0, USB_UPDATER_IDENTITY_AUTOMATIC, false,
                    "zpbr");
    assert_identity(USB_RUNTIME_MODE_PROTOCOL_BRIDGE, 0, 0x85, false, "zfor");
    assert_identity(USB_RUNTIME_MODE_PROTOCOL_BRIDGE, 0, 4, false, "r650");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 0x1b, 0x86, false, "zmcl");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 0, 7, false, "phub");
    assert_identity(USB_RUNTIME_MODE_USB_BRIDGE, 0, 0xfe, false, "FFFF");
}

static void test_ignores_invalid_destinations(void) {
    UsbUpdaterIdentityInput input = {0};
    uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE] = {0};
    usb_updater_identity_select(NULL, identity);
    usb_updater_identity_select(&input, NULL);
}

int main(void) {
    test_selects_runtime_identity();
    test_selects_usb_wheel_identity();
    test_selects_protocol_and_explicit_identity();
    test_ignores_invalid_destinations();
    return 0;
}
