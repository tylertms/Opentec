#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_fragment.h"

static const uint8_t first_packet[] = {0x03, 0x00, 0x04, 0x01, 0x41, 0x42, 0x43, 0xc0, 0x26};
static const uint8_t continuation_packet[] = {0x03, 0x00, 0x03, 0x02, 0x44, 0x45, 0xa8, 0xa7};
static const uint8_t final_packet[] = {0x03, 0x00, 0x02, 0x04, 0x46, 0xe8, 0x26};

static void test_assembles_fragmented_command(void) {
    static const uint8_t expected[] = {0x03, 0x00, 0x04, 0x01, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    uint8_t data[sizeof(expected)];
    MotorCommandFragment fragment;
    motor_command_fragment_init(&fragment, data, sizeof(data));

    assert(motor_command_fragment_accept(&fragment, first_packet, sizeof(first_packet)) ==
           MOTOR_COMMAND_FRAGMENT_WAITING);
    assert(fragment.length == 7);
    assert(fragment.content_length == 0);
    assert(motor_command_fragment_accept(&fragment, continuation_packet,
                                         sizeof(continuation_packet)) ==
           MOTOR_COMMAND_FRAGMENT_WAITING);
    assert(fragment.length == 9);
    assert(motor_command_fragment_accept(&fragment, final_packet, sizeof(final_packet)) ==
           MOTOR_COMMAND_FRAGMENT_COMPLETE);
    assert(fragment.length == sizeof(expected));
    assert(fragment.content_length == sizeof(expected) - 3);
    assert(memcmp(fragment.data, expected, sizeof(expected)) == 0);
}

static void test_restarts_with_new_first_fragment(void) {
    uint8_t data[16];
    MotorCommandFragment fragment;
    motor_command_fragment_init(&fragment, data, sizeof(data));

    assert(motor_command_fragment_accept(&fragment, first_packet, sizeof(first_packet)) ==
           MOTOR_COMMAND_FRAGMENT_WAITING);
    assert(motor_command_fragment_accept(&fragment, continuation_packet,
                                         sizeof(continuation_packet)) ==
           MOTOR_COMMAND_FRAGMENT_WAITING);
    assert(motor_command_fragment_accept(&fragment, first_packet, sizeof(first_packet)) ==
           MOTOR_COMMAND_FRAGMENT_WAITING);
    assert(fragment.length == 7);
}

static void test_rejects_invalid_fragments(void) {
    uint8_t data[8];
    uint8_t invalid[sizeof(first_packet)];
    MotorCommandFragment fragment;
    motor_command_fragment_init(&fragment, data, sizeof(data));

    assert(motor_command_fragment_accept(&fragment, continuation_packet,
                                         sizeof(continuation_packet)) ==
           MOTOR_COMMAND_FRAGMENT_INVALID);
    memcpy(invalid, first_packet, sizeof(invalid));
    invalid[sizeof(invalid) - 1] ^= 1;
    assert(motor_command_fragment_accept(&fragment, invalid, sizeof(invalid)) ==
           MOTOR_COMMAND_FRAGMENT_INVALID);
    assert(motor_command_fragment_accept(&fragment, first_packet, sizeof(first_packet)) ==
           MOTOR_COMMAND_FRAGMENT_WAITING);
    assert(motor_command_fragment_accept(&fragment, continuation_packet,
                                         sizeof(continuation_packet)) ==
           MOTOR_COMMAND_FRAGMENT_INVALID);
    assert(fragment.length == 7);
}

int main(void) {
    test_assembles_fragmented_command();
    test_restarts_with_new_first_fragment();
    test_rejects_invalid_fragments();
    return 0;
}
