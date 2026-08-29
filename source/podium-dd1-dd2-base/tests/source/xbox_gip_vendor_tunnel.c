#include "usb/xbox_gip_vendor_tunnel.h"

#include <assert.h>
#include <stdint.h>

static void test_decodes_operating_and_vendor_payloads(void) {
    uint8_t packet[64] = {[0] = 0x0f, [3] = 60, [4] = 0x35, [5] = 0xf8, [6] = 9};
    UsbOutputCommand command;

    assert(usb_xbox_gip_vendor_tunnel_decode(packet, sizeof(packet), &command));
    assert(command.kind == USB_OUTPUT_COMMAND_SHORT);
    assert(command.payload == packet + 5 && command.length == 7);

    packet[4] = 0x36;
    packet[5] = 5;
    assert(usb_xbox_gip_vendor_tunnel_decode(packet, sizeof(packet), &command));
    assert(command.kind == USB_OUTPUT_COMMAND_VENDOR_TRANSFER);
    assert(command.payload == packet + 5 && command.length == 59);
}

static void test_rejects_other_framing(void) {
    uint8_t packet[64] = {[0] = 0x0f, [3] = 60, [4] = 0x36};
    UsbOutputCommand command;

    packet[0] = 0x0e;
    assert(!usb_xbox_gip_vendor_tunnel_decode(packet, sizeof(packet), &command));
    packet[0] = 0x0f;
    packet[3] = 59;
    assert(!usb_xbox_gip_vendor_tunnel_decode(packet, sizeof(packet), &command));
    packet[3] = 60;
    packet[4] = 0x37;
    assert(!usb_xbox_gip_vendor_tunnel_decode(packet, sizeof(packet), &command));
    packet[4] = 0x36;
    assert(!usb_xbox_gip_vendor_tunnel_decode(packet, 63, &command));
    assert(!usb_xbox_gip_vendor_tunnel_decode(NULL, sizeof(packet), &command));
    assert(!usb_xbox_gip_vendor_tunnel_decode(packet, sizeof(packet), NULL));
}

int main(void) {
    test_decodes_operating_and_vendor_payloads();
    test_rejects_other_framing();
    return 0;
}
