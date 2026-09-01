#include "secure_element/a71ch.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static void test_accepts_handshake_and_clears_retry_count(void) {
    A71chStatusPoll handshake = {.retry_count = 3};

    assert(a71ch_status_poll_evaluate(&handshake, 0x07) == A71CH_STATUS_ACCEPTED);
    assert(handshake.retry_count == 0);
}

static void test_busy_handshake_increments_without_rejection(void) {
    A71chStatusPoll handshake;
    a71ch_status_poll_init(&handshake);

    for (uint8_t retry = 0; retry < 5; ++retry) {
        assert(a71ch_status_poll_evaluate(&handshake, 0x17) == A71CH_STATUS_BUSY);
    }
    assert(handshake.retry_count == 5);
}

static void test_rejects_fourth_unexpected_handshake_response(void) {
    A71chStatusPoll handshake;
    a71ch_status_poll_init(&handshake);

    assert(a71ch_status_poll_evaluate(&handshake, 0x41) == A71CH_STATUS_RETRY);
    assert(a71ch_status_poll_evaluate(&handshake, 0x42) == A71CH_STATUS_RETRY);
    assert(a71ch_status_poll_evaluate(&handshake, 0x43) == A71CH_STATUS_RETRY);
    assert(a71ch_status_poll_evaluate(&handshake, 0x44) == A71CH_STATUS_REJECTED);
    assert(handshake.retry_count == 3);
}

static void test_classifies_command_responses(void) {
    assert(a71ch_command_response_evaluate(0x07) == A71CH_STATUS_ACCEPTED);
    assert(a71ch_command_response_evaluate(0x17) == A71CH_STATUS_BUSY);
    assert(a71ch_command_response_evaluate(0x06) == A71CH_STATUS_REJECTED);
}

static void test_calculates_xor_checksum(void) {
    const uint8_t payload[] = {0xa5, 0x0f, 0x33, 0xc0};

    assert(a71ch_lrc(NULL, 0) == 0);
    assert(a71ch_lrc(payload, sizeof(payload)) == 0x59);
}

static void test_parses_unchecked_read_response(void) {
    static const uint8_t response[] = {0x84, 0x91, 0x12, 0x34, 0, 0};
    A71chAuthenticationInput input = {.chunk_length = 2};
    A71chAuthenticationFrame frame;
    A71chAuthenticationResponse parsed;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_READ, &input, &frame));
    assert(a71ch_authentication_response_parse(&frame, response, sizeof(response), &parsed) ==
           A71CH_VALID);
    assert(parsed.payload == response + 2);
    assert(parsed.payload_length == 2);
}

static void test_validates_checked_read_response_xor(void) {
    uint8_t response[] = {0x84, 0x91, 0x12, 0x34, 0x56, 0x70, 0, 0};
    A71chAuthenticationInput input = {.chunk_length = 4};
    A71chAuthenticationFrame frame;
    A71chAuthenticationResponse parsed;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_READ_LRC, &input, &frame));
    assert(frame.response_integrity_offset == 2);
    assert(frame.response_integrity_length == 6);
    assert(a71ch_authentication_response_parse(&frame, response, sizeof(response), &parsed) ==
           A71CH_VALID);
    assert(parsed.payload == response + 2);
    assert(parsed.payload_length == 4);

    response[3] ^= 1;
    assert(a71ch_authentication_response_parse(&frame, response, sizeof(response), &parsed) ==
           A71CH_LRC_ERROR);
}

static void test_rejects_malformed_transfer_response(void) {
    static const uint8_t response[] = {0, 0, 0, 0};
    A71chAuthenticationInput input = {.chunk_length = 0};
    A71chAuthenticationFrame frame;
    A71chAuthenticationResponse parsed;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_READ, &input, &frame));
    assert(a71ch_authentication_response_parse(&frame, response, sizeof(response) - 1, &parsed) ==
           A71CH_MALFORMED_RESPONSE);
    frame.response_payload_offset = sizeof(response);
    frame.response_payload_length = 1;
    assert(a71ch_authentication_response_parse(&frame, response, sizeof(response), &parsed) ==
           A71CH_MALFORMED_RESPONSE);
}

static void test_encodes_fixed_requests(void) {
    static const A71chControlRequest expected[] = {
        [A71CH_WAKE_UP] = {.selector = 0x0f},
        [A71CH_SOFT_RESET] = {.selector = 0x1f, .response_length = 2},
        [A71CH_READ_ANSWER_TO_RESET] = {.selector = 0x2f, .response_length = 0x1f},
        [A71CH_PARAMETER_EXCHANGE] = {.selector = 0xff, .response_length = 2},
        [A71CH_READ_STATUS] = {.selector = 0x07, .response_length = 2},
    };

    for (A71chCommand command = A71CH_WAKE_UP; command <= A71CH_READ_STATUS; ++command) {
        const A71chControlRequest *encoded = a71ch_control_request_lookup(command);
        A71chControlRequest request = {0};
        assert(encoded != 0);
        assert(encoded->selector == expected[command].selector);
        assert(encoded->response_length == expected[command].response_length);
        assert(a71ch_control_request_encode(command, &request));
        assert(request.selector == expected[command].selector);
        assert(request.response_length == expected[command].response_length);
    }
}

static void test_rejects_reserved_requests(void) {
    A71chControlRequest request = {.selector = 0xa5, .response_length = 0x5a};

    assert(a71ch_control_request_lookup(0) == 0);
    assert(!a71ch_control_request_encode(0, &request));
    assert(request.selector == 0xa5 && request.response_length == 0x5a);
    assert(a71ch_control_request_lookup((A71chCommand)UINT8_MAX) == 0);
    assert(!a71ch_control_request_encode((A71chCommand)UINT8_MAX, &request));
    assert(request.selector == 0xa5 && request.response_length == 0x5a);
}

static void test_encodes_checksum_free_chunk_write(void) {
    static const uint8_t chunk[] = {0x11, 0x22, 0x33, 0x44};
    static const uint8_t expected[] = {9, 0x80, 0x44, 0x40, 3, 4, 0x11, 0x22, 0x33, 0x44};
    A71chAuthenticationInput input = {
        .phase = 0x0a,
        .chunk_index = 3,
        .chunk = chunk,
        .chunk_length = sizeof(chunk),
    };
    A71chAuthenticationFrame frame;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_WRITE, &input, &frame));
    assert(frame.selector == 0x20);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 4);
    assert(frame.response_payload_length == 0);
}

static void test_encodes_checksum_free_chunk_read(void) {
    static const uint8_t expected[] = {5, 0x80, 0x46, 0x40, 0x11, 0x10};
    A71chAuthenticationInput input = {
        .phase = 5,
        .chunk_index = 0x11,
        .chunk_length = 0x10,
    };
    A71chAuthenticationFrame frame;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_READ, &input, &frame));
    assert(frame.selector == 0x50);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 0x14);
    assert(frame.response_payload_offset == 2);
    assert(frame.response_payload_length == 0x10);
}

static void test_encodes_checksum_free_transfer_finish(void) {
    static const uint8_t expected[] = {5, 0x80, 0x48, 0, 0, 0};
    A71chAuthenticationInput input = {.phase = 9};
    A71chAuthenticationFrame frame;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_FINALIZE, &input, &frame));
    assert(frame.selector == 0x10);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 4);
}

static void test_encodes_checked_chunk_write(void) {
    static const uint8_t chunk[] = {0x11, 0x22, 0x33, 0x44};
    static const uint8_t expected[] = {11, 0x80, 0x44, 0x40, 3, 4, 0x11, 0x22, 0x33, 0x44, 0, 0};
    A71chAuthenticationInput input = {
        .phase = 0x0a,
        .chunk_index = 3,
        .chunk = chunk,
        .chunk_length = sizeof(chunk),
    };
    A71chAuthenticationFrame frame;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_WRITE_LRC, &input, &frame));
    assert(frame.selector == 0x24);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 5);
    assert(frame.response_payload_length == 0);
}

static void test_encodes_checked_chunk_read(void) {
    static const uint8_t expected[] = {7, 0x80, 0x46, 0x40, 0x11, 0x10, 0x86, 0};
    A71chAuthenticationInput input = {
        .phase = 5,
        .chunk_index = 0x11,
        .chunk_length = 0x10,
    };
    A71chAuthenticationFrame frame;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_READ_LRC, &input, &frame));
    assert(frame.selector == 0x54);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 0x14);
    assert(frame.response_payload_offset == 2);
    assert(frame.response_payload_length == 0x10);
}

static void test_encodes_checked_transfer_finish(void) {
    static const uint8_t expected[] = {4, 0x80, 0x48, 0, 0};
    A71chAuthenticationInput input = {.phase = 9};
    A71chAuthenticationFrame frame;

    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_FINALIZE_LRC, &input, &frame));
    assert(frame.selector == 0x14);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 5);
}

static void test_rejects_invalid_chunk_transfer(void) {
    A71chAuthenticationInput input = {.chunk_length = A71CH_AUTHENTICATION_CHUNK_CAPACITY + 1};
    A71chAuthenticationFrame frame;

    assert(!a71ch_authentication_encode(A71CH_AUTHENTICATION_READ, &input, &frame));
    input.chunk_length = 1;
    assert(!a71ch_authentication_encode(A71CH_AUTHENTICATION_WRITE, &input, &frame));
    input.chunk_length = 0;
    assert(!a71ch_authentication_encode((A71chCommand)UINT8_MAX, &input, &frame));
}

static void test_sequences_standard_transfer(void) {
    A71chAuthenticationSequence sequence;
    A71chAuthenticationStep step;

    a71ch_authentication_sequence_init(&sequence, false);
    for (uint8_t index = 0; index < 4; ++index) {
        assert(a71ch_authentication_sequence_current(&sequence, &step));
        assert(step.command == A71CH_AUTHENTICATION_WRITE);
        assert(step.phase == index);
        assert(step.chunk_index == index);
        assert(step.buffer_offset == (uint16_t)index * 64);
        assert(step.chunk_length == 64);
        assert(a71ch_authentication_sequence_accept(&sequence));
    }

    for (uint8_t index = 0; index < 16; ++index) {
        assert(a71ch_authentication_sequence_current(&sequence, &step));
        assert(step.command == A71CH_AUTHENTICATION_READ);
        assert(step.phase == (uint8_t)(index + 4));
        assert(step.chunk_index == index);
        assert(step.buffer_offset == (uint16_t)index * 64);
        assert(step.chunk_length == 64);
        assert(a71ch_authentication_sequence_accept(&sequence));
    }

    assert(a71ch_authentication_sequence_current(&sequence, &step));
    assert(step.command == A71CH_AUTHENTICATION_READ);
    assert(step.phase == 20);
    assert(step.chunk_index == 16);
    assert(step.buffer_offset == 0x400);
    assert(step.chunk_length == 16);
    assert(a71ch_authentication_sequence_accept(&sequence));

    assert(a71ch_authentication_sequence_current(&sequence, &step));
    assert(step.command == A71CH_AUTHENTICATION_FINALIZE);
    assert(step.phase == 21);
    assert(step.chunk_index == 0);
    assert(step.chunk_length == 0);
    assert(a71ch_authentication_sequence_accept(&sequence));
    assert(!a71ch_authentication_sequence_current(&sequence, &step));
    assert(!a71ch_authentication_sequence_accept(&sequence));
}

static void test_sequences_checked_commands(void) {
    A71chAuthenticationSequence sequence;
    A71chAuthenticationStep step;

    a71ch_authentication_sequence_init(&sequence, true);
    assert(a71ch_authentication_sequence_current(&sequence, &step));
    assert(step.command == A71CH_AUTHENTICATION_WRITE_LRC);

    for (uint8_t index = 0; index < 4; ++index) {
        assert(a71ch_authentication_sequence_accept(&sequence));
    }
    assert(a71ch_authentication_sequence_current(&sequence, &step));
    assert(step.command == A71CH_AUTHENTICATION_READ_LRC);

    for (uint8_t index = 0; index < 17; ++index) {
        assert(a71ch_authentication_sequence_accept(&sequence));
    }
    assert(a71ch_authentication_sequence_current(&sequence, &step));
    assert(step.command == A71CH_AUTHENTICATION_FINALIZE_LRC);
}

static void test_completes_a71ch_exchange(void) {
    A71chExchange exchange;

    a71ch_exchange_init(&exchange);
    assert(exchange.stage == A71CH_EXCHANGE_WAIT_READY);
    assert(exchange.result == A71CH_EXCHANGE_PENDING);
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(exchange.stage == A71CH_EXCHANGE_QUEUE_COMMAND);
    assert(a71ch_exchange_command_queued(&exchange));
    assert(exchange.stage == A71CH_EXCHANGE_WAIT_ACCEPTANCE);
    assert(a71ch_exchange_status(&exchange, 0x17));
    assert(exchange.stage == A71CH_EXCHANGE_WAIT_ACCEPTANCE);
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(exchange.stage == A71CH_EXCHANGE_WAIT_RESPONSE);
    assert(a71ch_exchange_finalize(&exchange, A71CH_VALID));
    assert(exchange.stage == A71CH_EXCHANGE_COMPLETE);
    assert(exchange.result == A71CH_EXCHANGE_SUCCEEDED);
}

static void test_limits_unexpected_ready_responses(void) {
    A71chExchange exchange;

    a71ch_exchange_init(&exchange);
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        assert(a71ch_exchange_status(&exchange, 0x41));
        assert(exchange.stage == A71CH_EXCHANGE_WAIT_READY);
    }
    assert(a71ch_exchange_status(&exchange, 0x41));
    assert(exchange.stage == A71CH_EXCHANGE_FAILED);
    assert(exchange.result == A71CH_EXCHANGE_COMMAND_ERROR);

    a71ch_exchange_init(&exchange);
    for (uint8_t attempt = 0; attempt < 8; ++attempt) {
        assert(a71ch_exchange_status(&exchange, 0x17));
        assert(exchange.stage == A71CH_EXCHANGE_WAIT_READY);
    }
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(exchange.readiness.retry_count == 0);
}

static void test_rejects_command_acceptance(void) {
    A71chExchange exchange;

    a71ch_exchange_init(&exchange);
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(a71ch_exchange_command_queued(&exchange));
    assert(a71ch_exchange_status(&exchange, 0x06));
    assert(exchange.stage == A71CH_EXCHANGE_FAILED);
    assert(exchange.result == A71CH_EXCHANGE_COMMAND_ERROR);
}

static void test_classifies_final_response_errors(void) {
    A71chExchange exchange;

    a71ch_exchange_init(&exchange);
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(a71ch_exchange_command_queued(&exchange));
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(a71ch_exchange_finalize(&exchange, A71CH_LRC_ERROR));
    assert(exchange.result == A71CH_EXCHANGE_LRC_ERROR);

    a71ch_exchange_init(&exchange);
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(a71ch_exchange_command_queued(&exchange));
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(a71ch_exchange_finalize(&exchange, A71CH_MALFORMED_RESPONSE));
    assert(exchange.result == A71CH_EXCHANGE_RESPONSE_ERROR);
}

static void test_rejects_out_of_order_exchange_events(void) {
    A71chExchange exchange;

    a71ch_exchange_init(&exchange);
    assert(!a71ch_exchange_command_queued(&exchange));
    assert(!a71ch_exchange_finalize(&exchange, A71CH_VALID));
    assert(a71ch_exchange_status(&exchange, 0x07));
    assert(!a71ch_exchange_status(&exchange, 0x07));
}

static const uint8_t startup_signature[] = {
    0xb8, 0x04, 0x11, 0x01, 0x05, 0x04, 0xb9, 0x02, 0x01, 0x01, 0xba, 0x01, 0x01, 0xbb, 0x0c,
    0x41, 0x37, 0x31, 0x30, 0x35, 0x43, 0x43, 0x32, 0x34, 0x32, 0x52, 0x31, 0xbc, 0x00,
};

static void advance_startup_to_signature(A71chSession *startup) {
    A71chCommand command;
    A71chSessionResponse status = {.declared_length = 1};

    a71ch_session_init(startup);
    assert(a71ch_session_accept(startup, A71CH_WAKE_UP, NULL, 0));
    assert(a71ch_session_accept(startup, A71CH_SOFT_RESET, &status, 0));
    assert(!a71ch_session_current(startup, 4, &command));
    assert(a71ch_session_current(startup, 5, &command));
    assert(command == A71CH_READ_ANSWER_TO_RESET);
}

static void test_completes_a71ch_session(void) {
    A71chSession startup;
    A71chCommand command;
    A71chSessionResponse response = {0};

    a71ch_session_init(&startup);
    assert(a71ch_session_current(&startup, 0, &command));
    assert(command == A71CH_WAKE_UP);
    assert(a71ch_session_accept(&startup, command, NULL, 0));

    assert(a71ch_session_current(&startup, 0, &command));
    assert(command == A71CH_SOFT_RESET);
    response.declared_length = 1;
    assert(a71ch_session_accept(&startup, command, &response, 0));

    assert(!a71ch_session_current(&startup, 4, &command));
    assert(a71ch_session_current(&startup, 5, &command));
    assert(command == A71CH_READ_ANSWER_TO_RESET);
    response.payload = startup_signature;
    response.payload_length = sizeof(startup_signature);
    assert(a71ch_session_accept(&startup, command, &response, 0));

    assert(a71ch_session_current(&startup, 0, &command));
    assert(command == A71CH_PARAMETER_EXCHANGE);
    response.declared_length = 1;
    response.status = 0xcc;
    assert(a71ch_session_accept(&startup, command, &response, 0));

    assert(a71ch_session_current(&startup, 0, &command));
    assert(command == A71CH_READ_STATUS);
    response.status = 7;
    assert(a71ch_session_accept(&startup, command, &response, 0));
    assert(startup.complete);
    assert(!a71ch_session_current(&startup, 0, &command));
}

static void test_delays_and_limits_startup_status_retries(void) {
    A71chSession startup;
    A71chCommand command;
    A71chSessionResponse response = {.declared_length = 1, .status = 7};
    uint32_t now_ms = 100;

    a71ch_session_init(&startup);
    assert(a71ch_session_accept(&startup, A71CH_WAKE_UP, NULL, now_ms));
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        assert(a71ch_session_accept(&startup, A71CH_SOFT_RESET, &response, now_ms));
        assert(!a71ch_session_current(&startup, now_ms + 4, &command));
        now_ms += 5;
        assert(a71ch_session_current(&startup, now_ms, &command));
        assert(command == A71CH_SOFT_RESET);
    }

    assert(a71ch_session_accept(&startup, A71CH_SOFT_RESET, &response, now_ms));
    assert(startup.command == A71CH_WAKE_UP);
    assert(startup.completed_attempts == 0);
    assert(!startup.waiting);
}

static void test_retries_invalid_identity_and_confirmation(void) {
    uint8_t mismatched_signature[sizeof(startup_signature)];
    A71chSession startup;
    A71chSessionResponse response = {
        .payload = mismatched_signature,
        .payload_length = sizeof(mismatched_signature),
    };

    memcpy(mismatched_signature, startup_signature, sizeof(mismatched_signature));
    mismatched_signature[5] ^= 1;
    advance_startup_to_signature(&startup);
    assert(a71ch_session_accept(&startup, A71CH_READ_ANSWER_TO_RESET, &response, 0));
    assert(startup.command == A71CH_READ_ANSWER_TO_RESET);

    response.payload = startup_signature;
    assert(a71ch_session_accept(&startup, A71CH_READ_ANSWER_TO_RESET, &response, 0));
    assert(startup.command == A71CH_PARAMETER_EXCHANGE);
    assert(startup.completed_attempts == 0);

    response.declared_length = 1;
    response.status = 0xcb;
    assert(a71ch_session_accept(&startup, A71CH_PARAMETER_EXCHANGE, &response, 0));
    assert(startup.command == A71CH_PARAMETER_EXCHANGE);
    response.status = 0xcc;
    assert(a71ch_session_accept(&startup, A71CH_PARAMETER_EXCHANGE, &response, 0));
    assert(startup.command == A71CH_READ_STATUS);
}

static void test_restarts_on_negative_ready_status(void) {
    A71chSession startup;
    A71chSessionResponse response = {
        .payload = startup_signature,
        .payload_length = sizeof(startup_signature),
    };

    advance_startup_to_signature(&startup);
    assert(a71ch_session_accept(&startup, A71CH_READ_ANSWER_TO_RESET, &response, 0));
    response.declared_length = 1;
    response.status = 0xcc;
    assert(a71ch_session_accept(&startup, A71CH_PARAMETER_EXCHANGE, &response, 0));
    response.status = 6;
    assert(a71ch_session_accept(&startup, A71CH_READ_STATUS, &response, 0));
    assert(startup.command == A71CH_READ_STATUS);
    response.status = 0x80;
    assert(a71ch_session_accept(&startup, A71CH_READ_STATUS, &response, 0));
    assert(startup.command == A71CH_WAKE_UP);
}

static void test_rejects_out_of_order_startup_events(void) {
    A71chSession startup;
    A71chSessionResponse response = {.declared_length = 1, .status = 7};

    a71ch_session_init(&startup);
    assert(!a71ch_session_accept(&startup, A71CH_SOFT_RESET, &response, 0));
    assert(a71ch_session_accept(&startup, A71CH_WAKE_UP, NULL, 0));
    assert(a71ch_session_accept(&startup, A71CH_SOFT_RESET, &response, 0));
    assert(!a71ch_session_accept(&startup, A71CH_SOFT_RESET, &response, 1));
}

int main(void) {
    test_accepts_handshake_and_clears_retry_count();
    test_busy_handshake_increments_without_rejection();
    test_rejects_fourth_unexpected_handshake_response();
    test_classifies_command_responses();
    test_calculates_xor_checksum();
    test_parses_unchecked_read_response();
    test_validates_checked_read_response_xor();
    test_rejects_malformed_transfer_response();
    test_encodes_fixed_requests();
    test_rejects_reserved_requests();
    test_encodes_checksum_free_chunk_write();
    test_encodes_checksum_free_chunk_read();
    test_encodes_checksum_free_transfer_finish();
    test_encodes_checked_chunk_write();
    test_encodes_checked_chunk_read();
    test_encodes_checked_transfer_finish();
    test_rejects_invalid_chunk_transfer();
    test_sequences_standard_transfer();
    test_sequences_checked_commands();
    test_completes_a71ch_exchange();
    test_limits_unexpected_ready_responses();
    test_rejects_command_acceptance();
    test_classifies_final_response_errors();
    test_rejects_out_of_order_exchange_events();
    test_completes_a71ch_session();
    test_delays_and_limits_startup_status_retries();
    test_retries_invalid_identity_and_confirmation();
    test_restarts_on_negative_ready_status();
    test_rejects_out_of_order_startup_events();
    return 0;
}
