#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_channel.h"
#include "motor/command_packet.h"

enum { TEST_BUFFER_SIZE = 128 };

typedef struct {
    MotorCommandChannel channel;
    uint8_t receive_assembly[TEST_BUFFER_SIZE];
    uint8_t transmit[TEST_BUFFER_SIZE];
    uint8_t pending_payload[TEST_BUFFER_SIZE];
} Fixture;

static void fixture_init(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    MotorCommandChannelBuffers buffers = {
        .receive_assembly = fixture->receive_assembly,
        .receive_assembly_capacity = sizeof(fixture->receive_assembly),
        .transmit = fixture->transmit,
        .transmit_capacity = sizeof(fixture->transmit),
        .pending_payload = fixture->pending_payload,
        .pending_payload_capacity = sizeof(fixture->pending_payload),
    };
    assert(motor_command_channel_init(&fixture->channel, &buffers));
}

static void acknowledge(Fixture *fixture, uint8_t sequence) {
    uint8_t packet[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE];
    motor_command_packet_acknowledgement_encode(sequence, packet);
    (void)motor_command_channel_accept(&fixture->channel, packet, sizeof(packet));
}

static void retry(Fixture *fixture, uint8_t sequence) {
    uint8_t packet[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE];
    motor_command_packet_retry_encode(sequence, packet);
    (void)motor_command_channel_accept(&fixture->channel, packet, sizeof(packet));
}

static void test_reserves_sequence_before_transmission(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t payload[] = {0xc1, 0x12, 0x34};

    assert(motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload)));
    assert(fixture.channel.receiver.sequence.transmit == 1);
    assert(fixture.transmit[0] == 7);
    motor_command_channel_mark_written(&fixture.channel, fixture.transmit);
    assert(fixture.channel.receiver.sequence.transmit == 1);
    acknowledge(&fixture, 0);
    assert(!fixture.channel.command_pending);

    assert(motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload)));
    assert(fixture.channel.receiver.sequence.transmit == 2);
    assert(fixture.transmit[0] == 11);
}

static void test_resend_does_not_consume_retry_budget(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t payload[] = {0xc1, 0x12, 0x34};

    assert(motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload)));
    motor_command_channel_mark_written(&fixture.channel, fixture.transmit);
    acknowledge(&fixture, 2);
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(fixture.channel.retry_count == 0);
    assert(fixture.transmit[0] == 3);
}

static void test_retry_requeues_without_reset_budget(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t payload[] = {0xc1, 0x12, 0x34};

    assert(motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload)));
    motor_command_channel_mark_written(&fixture.channel, fixture.transmit);
    retry(&fixture, 1);
    assert(fixture.channel.retry_count == 0);
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(fixture.channel.pending_payload_length == sizeof(payload));
    assert(fixture.pending_payload[0] == payload[0]);
    retry(&fixture, 1);
    assert(fixture.channel.retry_count == 0);
    assert(fixture.channel.command_pending);
    assert(fixture.channel.pending_payload_length == sizeof(payload));
    assert(fixture.pending_payload[0] == payload[0]);
    assert(fixture.transmit[4] == payload[0]);
}

static void test_peer_reset_restarts_pending_command(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t payload[] = {0xc1, 0x12, 0x34};
    uint8_t packet[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE];

    assert(motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload)));
    motor_command_channel_mark_written(&fixture.channel, fixture.transmit);
    motor_command_packet_sequence_reset_encode(packet);
    MotorCommandChannelEvent event =
        motor_command_channel_accept(&fixture.channel, packet, sizeof(packet));
    assert(event.receive_result == MOTOR_COMMAND_RECEIVE_RESET);
    assert(event.actions == MOTOR_COMMAND_CHANNEL_ACTION_WRITE);
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(fixture.channel.retry_count == 1);
    assert(fixture.channel.receiver.sequence.transmit == 0);
    assert(fixture.transmit[0] == 3);

    event = motor_command_channel_accept(&fixture.channel, packet, sizeof(packet));
    assert(event.receive_result == MOTOR_COMMAND_RECEIVE_RESET);
    assert(event.actions == MOTOR_COMMAND_CHANNEL_ACTION_WRITE);
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(fixture.channel.retry_count == 0);
    assert(fixture.channel.pending_payload_length == 1);
    assert(fixture.pending_payload[0] == 0xfe);
    assert(fixture.transmit[0] == 3);
    assert(fixture.transmit[4] == 0xfe);
}

static void test_invalid_packet_always_requests_retry(void) {
    Fixture fixture;
    fixture_init(&fixture);

    MotorCommandChannelEvent event = motor_command_channel_accept(&fixture.channel, 0, 0);
    assert(event.receive_result == MOTOR_COMMAND_RECEIVE_INVALID);
    assert(event.actions == MOTOR_COMMAND_CHANNEL_ACTION_WRITE);
    assert(event.packet == fixture.channel.control_packet);
    assert(event.packet[0] == 0xa0);
}

static void test_recovery_command_retains_sequence_and_ownership(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t payload[] = {0xc1};

    assert(motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload)));
    assert(fixture.channel.receiver.sequence.transmit == 1);
    assert(motor_command_channel_queue_recovery_command(&fixture.channel));
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(!fixture.channel.reset_pending);
    assert(fixture.channel.receiver.sequence.transmit == 1);
    assert(fixture.channel.pending_payload_length == 1);
    assert(fixture.pending_payload[0] == 0xfe);
    assert(fixture.transmit[0] == 7);
    assert(fixture.transmit[4] == 0xfe);
}

int main(void) {
    test_reserves_sequence_before_transmission();
    test_resend_does_not_consume_retry_budget();
    test_retry_requeues_without_reset_budget();
    test_peer_reset_restarts_pending_command();
    test_invalid_packet_always_requests_retry();
    test_recovery_command_retains_sequence_and_ownership();
    return 0;
}
