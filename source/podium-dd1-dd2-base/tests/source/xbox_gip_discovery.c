#include "usb/xbox_gip_discovery.h"

#include <assert.h>
#include <stdint.h>

static void test_repeats_digest_after_strict_deadline(void) {
    UsbXboxGipDiscovery discovery;
    usb_xbox_gip_discovery_init(&discovery);

    assert(usb_xbox_gip_discovery_poll(&discovery, 0, 100) == USB_XBOX_GIP_DISCOVERY_DIGEST);
    assert(discovery.deadline == 600);
    assert(usb_xbox_gip_discovery_poll(&discovery, 0, 600) == USB_XBOX_GIP_DISCOVERY_IDLE);
    assert(discovery.phase == USB_XBOX_GIP_DISCOVERY_WAIT_FOR_REQUEST);
    assert(usb_xbox_gip_discovery_poll(&discovery, 0, 601) == USB_XBOX_GIP_DISCOVERY_IDLE);
    assert(discovery.phase == USB_XBOX_GIP_DISCOVERY_SEND_DIGEST);
    assert(usb_xbox_gip_discovery_poll(&discovery, 0, 602) == USB_XBOX_GIP_DISCOVERY_DIGEST);
    assert(discovery.deadline == 1102);
}

static void test_classifies_discovery_requests(void) {
    UsbXboxGipDiscovery discovery;
    usb_xbox_gip_discovery_init(&discovery);
    assert(usb_xbox_gip_discovery_poll(&discovery, 0, 0) == USB_XBOX_GIP_DISCOVERY_DIGEST);
    assert(usb_xbox_gip_discovery_poll(&discovery, 4, 1) == USB_XBOX_GIP_DISCOVERY_METADATA);
    assert(usb_xbox_gip_discovery_poll(&discovery, 5, 2) == USB_XBOX_GIP_DISCOVERY_SESSION_COMMAND);
}

int main(void) {
    test_repeats_digest_after_strict_deadline();
    test_classifies_discovery_requests();
    return 0;
}
