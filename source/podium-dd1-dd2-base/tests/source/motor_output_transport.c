#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/command.h"
#include "motor/output_transport.h"

static const ForceOutputReport live_report = {
    .positive_direction = true,
    .primary_magnitude = 0x1234,
    .secondary_magnitude = 0x5678,
};

static void test_status_precedes_live_output(void) {
    MotorOutputTransport transport;
    MotorLiveFrame frame;

    motor_output_transport_init(&transport);
    motor_output_transport_build_frame(&transport, MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS, 123,
                                       &live_report, &frame);

    assert(frame.type == MOTOR_LIVE_STATUS_TYPE);
    assert(frame.payload[0] == MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS);
    for (uint8_t index = 1; index < MOTOR_LIVE_PAYLOAD_SIZE; index++) {
        assert(frame.payload[index] == 0);
    }

    motor_output_transport_build_frame(&transport, MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS, 123,
                                       &live_report, &frame);
    assert(frame.type == MOTOR_LIVE_POSITION_TYPE);
    assert(frame.payload[0] == 123);
    assert(frame.payload[1] == 0);
    assert(frame.payload[2] == 1);
    assert(frame.payload[3] == 0x34);
    assert(frame.payload[4] == 0x12);
    assert(frame.payload[5] == 0x78);
    assert(frame.payload[6] == 0x56);
    assert(frame.payload[7] == 0);
}

static void test_commands_precede_status_changes(void) {
    const uint8_t command[MOTOR_OUTPUT_COMMAND_SIZE] = {0x11, 8, 3, 4, 5, 6, 7};
    MotorOutputTransport transport;
    MotorLiveFrame frame;

    motor_output_transport_init(&transport);
    assert(motor_output_transport_enqueue_command(&transport, command));
    motor_output_transport_build_frame(&transport, MOTOR_OUTPUT_STATUS_PRIMARY_DISABLED, 0,
                                       &live_report, &frame);

    assert(frame.type == MOTOR_LIVE_STATUS_TYPE);
    assert(frame.payload[0] == MOTOR_OUTPUT_STATUS_PRIMARY_DISABLED);
    assert(memcmp(frame.payload + 1, command, sizeof(command)) == 0);
    assert(transport.count == 0);

    motor_output_transport_build_frame(&transport, MOTOR_OUTPUT_STATUS_SECONDARY_DISABLED, 0,
                                       &live_report, &frame);
    assert(frame.type == MOTOR_LIVE_STATUS_TYPE);
    assert(frame.payload[0] == MOTOR_OUTPUT_STATUS_SECONDARY_DISABLED);
    for (uint8_t index = 1; index < MOTOR_LIVE_PAYLOAD_SIZE; index++) {
        assert(frame.payload[index] == 0);
    }
}

static void test_opcode_records_are_zero_filled(void) {
    MotorOutputTransport transport;
    MotorLiveFrame frame;

    motor_output_transport_init(&transport);
    assert(motor_output_transport_enqueue_opcode(&transport, 0x43));
    motor_output_transport_build_frame(&transport, 0, 0, &live_report, &frame);

    assert(frame.payload[1] == 0x43);
    for (uint8_t index = 2; index < MOTOR_LIVE_PAYLOAD_SIZE; index++) {
        assert(frame.payload[index] == 0);
    }
}

static void test_host_effect_clear_sequence(void) {
    MotorOutputTransport transport;
    MotorLiveFrame frame;

    motor_output_transport_init(&transport);
    assert(motor_output_transport_enqueue_host_effect_clears(&transport) == 16);
    for (uint8_t slot = 0; slot < 16; slot++) {
        motor_output_transport_build_frame(&transport, 0, 0, &live_report, &frame);
        assert(frame.payload[1] == ((uint8_t)(slot << 4) | 3u));
        for (uint8_t index = 2; index < MOTOR_LIVE_PAYLOAD_SIZE; index++) {
            assert(frame.payload[index] == 0);
        }
    }
    assert(transport.count == 0);
}

static void test_host_effect_clear_barrier_discards_stale_commands(void) {
    uint8_t stale_command[MOTOR_OUTPUT_COMMAND_SIZE] = {0x21, 8, 1, 2, 3, 4, 5};
    const uint8_t position_command[MOTOR_OUTPUT_COMMAND_SIZE] = {0x14, 0, 0, 0, 0, 0, 0};
    const uint8_t new_command[MOTOR_OUTPUT_COMMAND_SIZE] = {0x41, 8, 6, 7, 8, 9, 10};
    MotorOutputTransport transport;
    MotorLiveFrame frame;

    motor_output_transport_init(&transport);
    assert(motor_output_transport_enqueue_command(&transport, position_command));
    for (uint8_t index = 0; index < MOTOR_OUTPUT_QUEUE_CAPACITY - 1; index++) {
        stale_command[0] = (uint8_t)(0x01u | ((index & 0x0fu) << 4));
        assert(motor_output_transport_enqueue_command(&transport, stale_command));
    }
    assert(motor_output_transport_enqueue_host_effect_clears(&transport) == 16);
    assert(transport.count == 1);
    assert(motor_output_transport_enqueue_command(&transport, new_command));

    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_EFFECT_SLOT_COUNT; slot++) {
        motor_output_transport_build_frame(&transport, MOTOR_OUTPUT_STATUS_ENABLED, 0, &live_report,
                                           &frame);
        assert(frame.payload[0] == 0);
        assert(frame.payload[1] == ((uint8_t)(slot << 4) | 3u));
    }

    motor_output_transport_build_frame(&transport, MOTOR_OUTPUT_STATUS_ENABLED, 0, &live_report,
                                       &frame);
    assert(memcmp(frame.payload + 1, position_command, sizeof(position_command)) == 0);
    motor_output_transport_build_frame(&transport, MOTOR_OUTPUT_STATUS_ENABLED, 0, &live_report,
                                       &frame);
    assert(frame.payload[0] == MOTOR_OUTPUT_STATUS_ENABLED);
    assert(memcmp(frame.payload + 1, new_command, sizeof(new_command)) == 0);
    assert(transport.host_effect_clear_count == 0);
    assert(transport.count == 0);
}

static void test_queue_capacity_and_wrap(void) {
    MotorOutputTransport transport;
    MotorLiveFrame frame;
    uint8_t command[MOTOR_OUTPUT_COMMAND_SIZE] = {0};

    motor_output_transport_init(&transport);
    for (uint8_t index = 0; index < MOTOR_OUTPUT_QUEUE_CAPACITY; index++) {
        command[0] = index;
        assert(motor_output_transport_enqueue_command(&transport, command));
    }
    assert(!motor_output_transport_enqueue_opcode(&transport, 0xff));

    for (uint8_t index = 0; index < MOTOR_OUTPUT_QUEUE_CAPACITY; index++) {
        motor_output_transport_build_frame(&transport, 0, 0, &live_report, &frame);
        assert(frame.payload[1] == index);
    }
    assert(transport.count == 0);
    assert(transport.read_index == 0);
    assert(transport.write_index == 0);

    assert(motor_output_transport_enqueue_opcode(&transport, 0xa5));
    motor_output_transport_build_frame(&transport, 0, 0, &live_report, &frame);
    assert(frame.payload[1] == 0xa5);
}

static void test_replays_the_older_retained_frame(void) {
    MotorOutputTransport transport;
    MotorLiveFrame first = {.type = 0x11, .payload = {1}};
    MotorLiveFrame second = {.type = 0x22, .payload = {2}};
    MotorLiveFrame third = {.type = 0x33, .payload = {3}};
    MotorLiveFrame replay = {0};

    motor_output_transport_init(&transport);
    assert(!motor_output_transport_replay_frame(&transport, &replay));
    motor_output_transport_remember_frame(&transport, &first);
    assert(motor_output_transport_replay_frame(&transport, &replay));
    assert(memcmp(&replay, &first, sizeof(replay)) == 0);

    motor_output_transport_remember_frame(&transport, &second);
    assert(motor_output_transport_replay_frame(&transport, &replay));
    assert(memcmp(&replay, &first, sizeof(replay)) == 0);

    motor_output_transport_remember_frame(&transport, &third);
    assert(motor_output_transport_replay_frame(&transport, &replay));
    assert(memcmp(&replay, &second, sizeof(replay)) == 0);
}

static void test_replay_does_not_consume_commands(void) {
    MotorOutputTransport transport;
    MotorLiveFrame retained = {.type = 0x44, .payload = {4}};
    MotorLiveFrame replay;

    motor_output_transport_init(&transport);
    motor_output_transport_remember_frame(&transport, &retained);
    assert(motor_output_transport_enqueue_opcode(&transport, 0xa5));
    assert(motor_output_transport_replay_frame(&transport, &replay));
    assert(transport.count == 1);
    assert(transport.previous_status == 0);
}

int main(void) {
    test_status_precedes_live_output();
    test_commands_precede_status_changes();
    test_opcode_records_are_zero_filled();
    test_host_effect_clear_sequence();
    test_host_effect_clear_barrier_discards_stale_commands();
    test_queue_capacity_and_wrap();
    test_replays_the_older_retained_frame();
    test_replay_does_not_consume_commands();
    return 0;
}
