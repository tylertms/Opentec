#include "usb/xbox_gip_command.h"

#include <assert.h>
#include <stdint.h>

static void test_decodes_script_queries(void) {
    uint8_t packet[7] = {0x0a, 0, 0, 0, 4, 0xf5, 1};
    UsbXboxGipCommand command;

    assert(usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    assert(command.kind == USB_XBOX_GIP_COMMAND_SCRIPT_SAMPLES && command.parameter == 501);

    static const UsbXboxGipCommandKind expected[] = {
        USB_XBOX_GIP_COMMAND_SCRIPT_SLOT,
        USB_XBOX_GIP_COMMAND_SCRIPT_STATUS,
        USB_XBOX_GIP_COMMAND_SCRIPT_VALUES,
        USB_XBOX_GIP_COMMAND_SCRIPT_AXES,
    };
    for (uint8_t selector = 5; selector <= 8; selector++) {
        packet[4] = selector;
        packet[5] = selector == 5 ? 15 : 0x34;
        packet[6] = selector == 5 ? 0 : 0x12;
        assert(usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
        assert(command.kind == expected[selector - 5]);
        assert(command.parameter == (selector == 5 ? 15 : 0x1234));
    }
}

static void test_rejects_other_packets_and_invalid_query_ranges(void) {
    uint8_t packet[7] = {0x0a, 0, 0, 0, 4, 0xf6, 1};
    UsbXboxGipCommand command;

    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[4] = 5;
    packet[5] = 16;
    packet[6] = 0;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[4] = 3;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[4] = 6;
    packet[1] = 0x20;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[1] = 0;
    packet[0] = 0x0b;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    assert(!usb_xbox_gip_command_decode(packet, 6, &command));
    assert(!usb_xbox_gip_command_decode(NULL, sizeof(packet), &command));
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), NULL));
}

int main(void) {
    test_decodes_script_queries();
    test_rejects_other_packets_and_invalid_query_ranges();
    return 0;
}
