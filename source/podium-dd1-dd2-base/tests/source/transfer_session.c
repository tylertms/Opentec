#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "transfer/frame.h"
#include "transfer/session.h"

typedef struct {
    uint8_t sent[TRANSFER_FRAME_MAX_ENCODED_SIZE];
    uint8_t received[TRANSFER_FRAME_MAX_PAYLOAD_SIZE];
    uint32_t now;
    uint16_t sent_length;
    uint8_t received_length;
    uint8_t received_group;
    uint8_t send_count;
    uint8_t receive_count;
    bool ready;
    bool complete;
} Fixture;

static void send_data(void *context, const uint8_t *data, uint16_t length) {
    Fixture *fixture = context;
    memcpy(fixture->sent, data, length);
    fixture->sent_length = length;
    fixture->send_count++;
}

static bool is_ready(void *context) {
    Fixture *fixture = context;
    return fixture->ready;
}

static void receive_data(void *context, const uint8_t *data, uint8_t length, uint8_t group,
                         bool complete) {
    Fixture *fixture = context;
    memcpy(fixture->received, data, length);
    fixture->received_length = length;
    fixture->received_group = group;
    fixture->complete = complete;
    fixture->receive_count++;
}

static uint32_t read_clock(void *context) {
    Fixture *fixture = context;
    return fixture->now;
}

static TransferSessionCallbacks callbacks(void) {
    return (TransferSessionCallbacks){
        .send = send_data,
        .ready = is_ready,
        .data = receive_data,
        .clock = read_clock,
    };
}

static uint16_t encode(uint16_t command, const uint8_t *payload, uint8_t payload_length,
                       uint8_t output[TRANSFER_FRAME_MAX_ENCODED_SIZE]) {
    TransferFrame frame = {
        .command = command,
        .payload_length = payload_length,
    };
    if (payload_length != 0) {
        memcpy(frame.payload, payload, payload_length);
    }
    return transfer_frame_encode(&frame, output);
}

static TransferFrame decode_sent(const Fixture *fixture) {
    TransferFrame frame;
    assert(transfer_frame_decode(fixture->sent, fixture->sent_length, &frame) ==
           TRANSFER_FRAME_VALID);
    return frame;
}

static void test_requires_all_callbacks(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();

    assert(!transfer_session_init(NULL, &valid, &fixture));
    assert(!transfer_session_init(&session, NULL, &fixture));

    valid.clock = NULL;
    assert(!transfer_session_init(&session, &valid, &fixture));
}

static void test_sends_single_data_and_accepts_status(void) {
    TransferSession session;
    Fixture fixture = {.now = 100};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t payload[] = {0x12, 0x34};
    uint8_t status[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));
    assert(transfer_session_send(&session, payload, sizeof(payload), 2));
    assert(session.outbound_pending);

    TransferFrame sent = decode_sent(&fixture);
    assert(sent.command == transfer_data_command(2, 0, 0));
    assert(sent.payload_length == sizeof(payload));
    assert(memcmp(sent.payload, payload, sizeof(payload)) == 0);
    assert(!transfer_session_send(&session, payload, sizeof(payload), 2));

    uint16_t status_length = encode(transfer_status_command(2, 0), NULL, 0, status);
    assert(transfer_session_receive(&session, status, status_length) == TRANSFER_SESSION_OK);
    assert(!session.outbound_pending);
}

static void test_acknowledges_single_inbound_data(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t payload[] = {0xaa, 0xbb};
    uint8_t frame[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));
    uint16_t frame_length = encode(transfer_data_command(3, 0, 5), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, frame_length) == TRANSFER_SESSION_OK);
    assert(fixture.receive_count == 1);
    assert(fixture.received_group == 3);
    assert(fixture.complete);
    assert(fixture.received_length == sizeof(payload));
    assert(memcmp(fixture.received, payload, sizeof(payload)) == 0);

    TransferFrame acknowledgment = decode_sent(&fixture);
    assert(acknowledgment.command == transfer_status_command(3, 5));
    assert(acknowledgment.payload_length == 0);
}

static void test_tracks_segment_sequences(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t payload[] = {0x55};
    uint8_t frame[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));

    uint16_t length = encode(transfer_data_command(1, 1, 6), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_OK);
    assert(!fixture.complete);

    length = encode(transfer_data_command(1, 2, 7), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_OK);
    assert(!fixture.complete);

    length = encode(transfer_data_command(1, 3, 1), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_OK);
    assert(fixture.complete);
    assert(!session.receive_active);
    assert(fixture.receive_count == 3);
}

static void test_single_frame_cancels_segmented_receive(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t payload[] = {0x55};
    uint8_t frame[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));
    uint16_t length = encode(transfer_data_command(1, 1, 6), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_OK);
    assert(session.receive_active);

    length = encode(transfer_data_command(1, 0, 4), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_OK);
    assert(fixture.complete);
    assert(!session.receive_active);

    length = encode(transfer_data_command(1, 1, 2), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_OK);
    assert(!fixture.complete);
    assert(session.receive_active);
}

static void test_accepts_reserved_sequence_as_incomplete(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t payload[] = {0x55};
    uint8_t frame[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));
    uint16_t length = encode(transfer_data_command(2, 4, 5), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_OK);
    assert(fixture.receive_count == 1);
    assert(!fixture.complete);
    assert(!session.receive_active);

    TransferFrame acknowledgment = decode_sent(&fixture);
    assert(acknowledgment.command == transfer_status_command(2, 0));
}

static void test_rejects_bad_sequence_without_immediate_shutdown(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();
    uint8_t frame[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));
    uint16_t length = encode(transfer_data_command(0, 2, 1), NULL, 0, frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_SEQUENCE_ERROR);
    assert(session.active);
    assert(fixture.receive_count == 0);
}

static void test_busy_transport_defers_send_and_acknowledgement(void) {
    TransferSession session;
    Fixture fixture = {.ready = true};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t payload[] = {0x55};
    uint8_t frame[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));
    assert(!transfer_session_send(&session, payload, sizeof(payload), 0));
    assert(!session.outbound_pending);

    uint16_t length = encode(transfer_data_command(0, 0, 0), payload, sizeof(payload), frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_BUSY);
    assert(fixture.receive_count == 1);
    assert(fixture.send_count == 0);
}

static void test_enforces_strict_deadlines(void) {
    TransferSession session;
    Fixture fixture = {.now = 100};
    TransferSessionCallbacks valid = callbacks();

    assert(transfer_session_init(&session, &valid, &fixture));
    fixture.now = 300;
    assert(transfer_session_poll(&session) == TRANSFER_SESSION_OK);
    fixture.now = 301;
    assert(transfer_session_poll(&session) == TRANSFER_SESSION_TIMED_OUT);
    assert(!session.active);
    assert(transfer_session_poll(&session) == TRANSFER_SESSION_INACTIVE);
}

static void test_keepalive_refreshes_both_deadlines(void) {
    TransferSession session;
    Fixture fixture = {.now = 100};
    TransferSessionCallbacks valid = callbacks();
    assert(transfer_session_init(&session, &valid, &fixture));

    fixture.now = 250;
    assert(transfer_session_keepalive(&session));
    TransferFrame keepalive = decode_sent(&fixture);
    assert(keepalive.command == transfer_empty_command());
    assert(keepalive.payload_length == 0);
    assert(session.activity_deadline == 450);
    assert(session.data_deadline == 450);

    fixture.ready = true;
    fixture.now = 300;
    assert(!transfer_session_keepalive(&session));
    assert(session.activity_deadline == 500);
    assert(session.data_deadline == 500);
}

static void test_stops_after_repeated_frame_errors(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t invalid[] = {0x3c, 0, 0, 0, 0x3e};

    assert(transfer_session_init(&session, &valid, &fixture));
    for (uint8_t attempt = 0; attempt < 4; attempt++) {
        assert(transfer_session_receive(&session, invalid, sizeof(invalid)) ==
               TRANSFER_SESSION_INVALID_FRAME);
        assert(session.active);
    }
    assert(transfer_session_receive(&session, invalid, sizeof(invalid)) ==
           TRANSFER_SESSION_INVALID_FRAME);
    assert(!session.active);

    TransferFrame error = decode_sent(&fixture);
    assert(error.command == transfer_progress_command(0, 0, 0));
}

static void test_remote_progress_error_stops_session(void) {
    TransferSession session;
    Fixture fixture = {0};
    TransferSessionCallbacks valid = callbacks();
    const uint8_t payload[] = {0x55};
    uint8_t frame[TRANSFER_FRAME_MAX_ENCODED_SIZE];

    assert(transfer_session_init(&session, &valid, &fixture));
    assert(transfer_session_send(&session, payload, sizeof(payload), 0));
    uint16_t length = encode(transfer_progress_command(0, 0, 1), NULL, 0, frame);
    assert(transfer_session_receive(&session, frame, length) == TRANSFER_SESSION_REMOTE_ERROR);
    assert(!session.active);
}

int main(void) {
    test_requires_all_callbacks();
    test_sends_single_data_and_accepts_status();
    test_acknowledges_single_inbound_data();
    test_tracks_segment_sequences();
    test_single_frame_cancels_segmented_receive();
    test_accepts_reserved_sequence_as_incomplete();
    test_rejects_bad_sequence_without_immediate_shutdown();
    test_busy_transport_defers_send_and_acknowledgement();
    test_enforces_strict_deadlines();
    test_keepalive_refreshes_both_deadlines();
    test_stops_after_repeated_frame_errors();
    test_remote_progress_error_stops_session();
    return 0;
}
