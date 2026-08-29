#include "usb/xbox_gip_command.h"

#include <assert.h>
#include <stdint.h>

static void test_decodes_control_commands(void) {
    uint8_t packet[7] = {0x0a, 0, 0, 0, 0, 0x34, 0x12};
    UsbXboxGipCommand command;
    static const UsbXboxGipCommandKind expected[] = {
        USB_XBOX_GIP_COMMAND_CAPABILITIES,
        USB_XBOX_GIP_COMMAND_STEERING_RANGE,
        USB_XBOX_GIP_COMMAND_FORCE_FEEDBACK_STRENGTH,
        USB_XBOX_GIP_COMMAND_TRANSFER_STATUS,
    };

    for (uint8_t selector = 0; selector <= 3; selector++) {
        packet[4] = selector;
        assert(usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
        assert(command.kind == expected[selector]);
        assert(command.parameter == (selector == 2 ? 0x34 : 0x1234));
    }

    packet[1] = 0x20;
    packet[4] = 1;
    assert(usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    assert(command.kind == USB_XBOX_GIP_COMMAND_TRANSFER_STATUS && command.parameter == 0x1234);
}

static void test_normalizes_control_values(void) {
    assert(usb_xbox_gip_steering_range_normalize(0) == 90);
    assert(usb_xbox_gip_steering_range_normalize(89) == 90);
    assert(usb_xbox_gip_steering_range_normalize(90) == 90);
    assert(usb_xbox_gip_steering_range_normalize(109) == 100);
    assert(usb_xbox_gip_steering_range_normalize(1080) == 1080);
    assert(usb_xbox_gip_steering_range_normalize(1081) == 1080);

    assert(usb_xbox_gip_force_feedback_strength_normalize(0) == 0);
    assert(usb_xbox_gip_force_feedback_strength_normalize(1) == 0);
    assert(usb_xbox_gip_force_feedback_strength_normalize(0x80) == 50);
    assert(usb_xbox_gip_force_feedback_strength_normalize(0xfe) == 99);
    assert(usb_xbox_gip_force_feedback_strength_normalize(0xff) == 100);
}

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

    packet[4] = 9;
    assert(usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    assert(command.kind == USB_XBOX_GIP_COMMAND_EXTENDED_STATUS);
}

static void test_rejects_other_packets_and_invalid_query_ranges(void) {
    uint8_t packet[7] = {0x0a, 0, 0, 0, 4, 0xf6, 1};
    UsbXboxGipCommand command;

    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[4] = 5;
    packet[5] = 16;
    packet[6] = 0;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[4] = 10;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[4] = 6;
    packet[1] = 0x20;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[4] = 1;
    packet[1] = 0x21;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    packet[1] = 0;
    packet[0] = 0x0b;
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), &command));
    assert(!usb_xbox_gip_command_decode(packet, 6, &command));
    assert(!usb_xbox_gip_command_decode(NULL, sizeof(packet), &command));
    assert(!usb_xbox_gip_command_decode(packet, sizeof(packet), NULL));
}

int main(void) {
    test_decodes_control_commands();
    test_normalizes_control_values();
    test_decodes_script_queries();
    test_rejects_other_packets_and_invalid_query_ranges();
    return 0;
}
