#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/probe.h"

static void test_accepts_handshake_and_clears_retry_count(void) {
    I2cProbeHandshake handshake = {.retry_count = 3};

    assert(i2c_probe_handshake_evaluate(&handshake, 0x07) == I2C_PROBE_RESPONSE_ACCEPTED);
    assert(handshake.retry_count == 0);
}

static void test_busy_handshake_increments_without_rejection(void) {
    I2cProbeHandshake handshake;
    i2c_probe_handshake_init(&handshake);

    for (uint8_t retry = 0; retry < 5; ++retry) {
        assert(i2c_probe_handshake_evaluate(&handshake, 0x17) == I2C_PROBE_RESPONSE_BUSY);
    }
    assert(handshake.retry_count == 5);
}

static void test_rejects_fourth_unexpected_handshake_response(void) {
    I2cProbeHandshake handshake;
    i2c_probe_handshake_init(&handshake);

    assert(i2c_probe_handshake_evaluate(&handshake, 0x41) == I2C_PROBE_RESPONSE_RETRY);
    assert(i2c_probe_handshake_evaluate(&handshake, 0x42) == I2C_PROBE_RESPONSE_RETRY);
    assert(i2c_probe_handshake_evaluate(&handshake, 0x43) == I2C_PROBE_RESPONSE_RETRY);
    assert(i2c_probe_handshake_evaluate(&handshake, 0x44) == I2C_PROBE_RESPONSE_REJECTED);
    assert(handshake.retry_count == 3);
}

static void test_classifies_command_responses(void) {
    assert(i2c_probe_command_response_evaluate(0x07) == I2C_PROBE_RESPONSE_ACCEPTED);
    assert(i2c_probe_command_response_evaluate(0x17) == I2C_PROBE_RESPONSE_BUSY);
    assert(i2c_probe_command_response_evaluate(0x06) == I2C_PROBE_RESPONSE_REJECTED);
}

static void test_calculates_xor_checksum(void) {
    const uint8_t payload[] = {0xa5, 0x0f, 0x33, 0xc0};

    assert(i2c_probe_checksum(NULL, 0) == 0);
    assert(i2c_probe_checksum(payload, sizeof(payload)) == 0x59);
}

static void test_rejects_nonzero_final_status_before_checksum(void) {
    const uint8_t payload[] = {0x12, 0x34};
    I2cProbeFinalResponse response = {
        .payload = payload,
        .payload_length = sizeof(payload),
        .expected_checksum = 0x26,
        .primary_status = 1,
    };

    assert(i2c_probe_final_response_validate(&response, true) == I2C_PROBE_STATUS_ERROR);
    response.primary_status = 0;
    response.secondary_status = 1;
    assert(i2c_probe_final_response_validate(&response, true) == I2C_PROBE_STATUS_ERROR);
}

static void test_optionally_validates_final_checksum(void) {
    const uint8_t payload[] = {0x12, 0x34};
    I2cProbeFinalResponse response = {
        .payload = payload,
        .payload_length = sizeof(payload),
        .expected_checksum = 0xff,
    };

    assert(i2c_probe_final_response_validate(&response, false) == I2C_PROBE_VALID);
    assert(i2c_probe_final_response_validate(&response, true) == I2C_PROBE_CHECKSUM_ERROR);
    response.expected_checksum = 0x26;
    assert(i2c_probe_final_response_validate(&response, true) == I2C_PROBE_VALID);
}

static void test_encodes_fixed_requests(void) {
    static const I2cProbeRequest expected[] = {
        [I2C_PROBE_BEGIN_SESSION] = {.selector = 0x0f},
        [I2C_PROBE_READ_STARTUP_STATUS] = {.selector = 0x1f, .response_length = 2},
        [I2C_PROBE_READ_SIGNATURE] = {.selector = 0x2f, .response_length = 0x1f},
        [I2C_PROBE_READ_CONFIRMATION] = {.selector = 0xff, .response_length = 2},
        [I2C_PROBE_READ_READY_STATUS] = {.selector = 0x07, .response_length = 2},
    };

    for (I2cProbeCommand command = I2C_PROBE_BEGIN_SESSION; command <= I2C_PROBE_READ_READY_STATUS;
         ++command) {
        I2cProbeRequest request = {0};
        assert(i2c_probe_request_encode(command, &request));
        assert(request.selector == expected[command].selector);
        assert(request.response_length == expected[command].response_length);
    }
}

static void test_rejects_reserved_requests(void) {
    I2cProbeRequest request = {.selector = 0xa5, .response_length = 0x5a};

    assert(!i2c_probe_request_encode(0, &request));
    assert(request.selector == 0xa5 && request.response_length == 0x5a);
    assert(!i2c_probe_request_encode(6, &request));
    assert(request.selector == 0xa5 && request.response_length == 0x5a);
}

static void test_encodes_checksum_free_chunk_write(void) {
    static const uint8_t chunk[] = {0x11, 0x22, 0x33, 0x44};
    static const uint8_t expected[] = {9, 0x80, 0x44, 0x40, 3, 4, 0x11, 0x22, 0x33, 0x44};
    I2cProbeTransferInput input = {
        .phase = 0x0a,
        .chunk_index = 3,
        .chunk = chunk,
        .chunk_length = sizeof(chunk),
    };
    I2cProbeTransferFrame frame;

    assert(i2c_probe_transfer_encode(I2C_PROBE_WRITE_CHUNK, &input, &frame));
    assert(frame.selector == 0x20);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 4);
    assert(frame.response_payload_length == 0);
}

static void test_encodes_checksum_free_chunk_read(void) {
    static const uint8_t expected[] = {5, 0x80, 0x46, 0x40, 0x11, 0x10};
    I2cProbeTransferInput input = {
        .phase = 5,
        .chunk_index = 0x11,
        .chunk_length = 0x10,
    };
    I2cProbeTransferFrame frame;

    assert(i2c_probe_transfer_encode(I2C_PROBE_READ_CHUNK, &input, &frame));
    assert(frame.selector == 0x50);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 0x14);
    assert(frame.response_payload_offset == 2);
    assert(frame.response_payload_length == 0x10);
}

static void test_encodes_checksum_free_transfer_finish(void) {
    static const uint8_t expected[] = {5, 0x80, 0x48, 0, 0, 0};
    I2cProbeTransferInput input = {.phase = 9};
    I2cProbeTransferFrame frame;

    assert(i2c_probe_transfer_encode(I2C_PROBE_FINISH_TRANSFER, &input, &frame));
    assert(frame.selector == 0x10);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 4);
}

static void test_encodes_checked_chunk_write(void) {
    static const uint8_t chunk[] = {0x11, 0x22, 0x33, 0x44};
    static const uint8_t expected[] = {11, 0x80, 0x44, 0x40, 3, 4, 0x11, 0x22, 0x33, 0x44, 0, 0};
    I2cProbeTransferInput input = {
        .phase = 0x0a,
        .chunk_index = 3,
        .chunk = chunk,
        .chunk_length = sizeof(chunk),
    };
    I2cProbeTransferFrame frame;

    assert(i2c_probe_transfer_encode(I2C_PROBE_WRITE_CHECKED_CHUNK, &input, &frame));
    assert(frame.selector == 0x24);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 5);
    assert(frame.response_payload_length == 0);
}

static void test_encodes_checked_chunk_read(void) {
    static const uint8_t expected[] = {7, 0x80, 0x46, 0x40, 0x11, 0x10, 0x86, 0};
    I2cProbeTransferInput input = {
        .phase = 5,
        .chunk_index = 0x11,
        .chunk_length = 0x10,
    };
    I2cProbeTransferFrame frame;

    assert(i2c_probe_transfer_encode(I2C_PROBE_READ_CHECKED_CHUNK, &input, &frame));
    assert(frame.selector == 0x54);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 0x14);
    assert(frame.response_payload_offset == 2);
    assert(frame.response_payload_length == 0x10);
}

static void test_encodes_checked_transfer_finish(void) {
    static const uint8_t expected[] = {4, 0x80, 0x48, 0, 0};
    I2cProbeTransferInput input = {.phase = 9};
    I2cProbeTransferFrame frame;

    assert(i2c_probe_transfer_encode(I2C_PROBE_FINISH_CHECKED_TRANSFER, &input, &frame));
    assert(frame.selector == 0x14);
    assert(frame.write_length == sizeof(expected));
    assert(memcmp(frame.write_data, expected, sizeof(expected)) == 0);
    assert(frame.response_length == 5);
}

static void test_rejects_invalid_chunk_transfer(void) {
    I2cProbeTransferInput input = {.chunk_length = I2C_PROBE_TRANSFER_CHUNK_CAPACITY + 1};
    I2cProbeTransferFrame frame;

    assert(!i2c_probe_transfer_encode(I2C_PROBE_READ_CHUNK, &input, &frame));
    input.chunk_length = 1;
    assert(!i2c_probe_transfer_encode(I2C_PROBE_WRITE_CHUNK, &input, &frame));
    input.chunk_length = 0;
    assert(!i2c_probe_transfer_encode(6, &input, &frame));
}

static void test_sequences_standard_transfer(void) {
    I2cProbeTransferSequence sequence;
    I2cProbeTransferStep step;

    i2c_probe_transfer_sequence_init(&sequence, false);
    for (uint8_t index = 0; index < 4; ++index) {
        assert(i2c_probe_transfer_sequence_current(&sequence, &step));
        assert(step.command == I2C_PROBE_WRITE_CHUNK);
        assert(step.phase == index);
        assert(step.chunk_index == index);
        assert(step.buffer_offset == (uint16_t)index * 64);
        assert(step.chunk_length == 64);
        assert(i2c_probe_transfer_sequence_accept(&sequence));
    }

    for (uint8_t index = 0; index < 16; ++index) {
        assert(i2c_probe_transfer_sequence_current(&sequence, &step));
        assert(step.command == I2C_PROBE_READ_CHUNK);
        assert(step.phase == (uint8_t)(index + 4));
        assert(step.chunk_index == index);
        assert(step.buffer_offset == (uint16_t)index * 64);
        assert(step.chunk_length == 64);
        assert(i2c_probe_transfer_sequence_accept(&sequence));
    }

    assert(i2c_probe_transfer_sequence_current(&sequence, &step));
    assert(step.command == I2C_PROBE_READ_CHUNK);
    assert(step.phase == 20);
    assert(step.chunk_index == 16);
    assert(step.buffer_offset == 0x400);
    assert(step.chunk_length == 16);
    assert(i2c_probe_transfer_sequence_accept(&sequence));

    assert(i2c_probe_transfer_sequence_current(&sequence, &step));
    assert(step.command == I2C_PROBE_FINISH_TRANSFER);
    assert(step.phase == 21);
    assert(step.chunk_index == 0);
    assert(step.chunk_length == 0);
    assert(i2c_probe_transfer_sequence_accept(&sequence));
    assert(!i2c_probe_transfer_sequence_current(&sequence, &step));
    assert(!i2c_probe_transfer_sequence_accept(&sequence));
}

static void test_sequences_checked_commands(void) {
    I2cProbeTransferSequence sequence;
    I2cProbeTransferStep step;

    i2c_probe_transfer_sequence_init(&sequence, true);
    assert(i2c_probe_transfer_sequence_current(&sequence, &step));
    assert(step.command == I2C_PROBE_WRITE_CHECKED_CHUNK);

    for (uint8_t index = 0; index < 4; ++index) {
        assert(i2c_probe_transfer_sequence_accept(&sequence));
    }
    assert(i2c_probe_transfer_sequence_current(&sequence, &step));
    assert(step.command == I2C_PROBE_READ_CHECKED_CHUNK);

    for (uint8_t index = 0; index < 17; ++index) {
        assert(i2c_probe_transfer_sequence_accept(&sequence));
    }
    assert(i2c_probe_transfer_sequence_current(&sequence, &step));
    assert(step.command == I2C_PROBE_FINISH_CHECKED_TRANSFER);
}

static void test_completes_probe_exchange(void) {
    static const uint8_t payload[] = {0x12, 0x34, 0x26};
    I2cProbeFinalResponse response = {
        .payload = payload,
        .payload_length = sizeof(payload),
        .expected_checksum = 0,
    };
    I2cProbeExchange exchange;

    i2c_probe_exchange_init(&exchange);
    assert(exchange.stage == I2C_PROBE_EXCHANGE_WAIT_READY);
    assert(exchange.result == I2C_PROBE_EXCHANGE_PENDING);
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(exchange.stage == I2C_PROBE_EXCHANGE_QUEUE_COMMAND);
    assert(i2c_probe_exchange_command_queued(&exchange));
    assert(exchange.stage == I2C_PROBE_EXCHANGE_WAIT_ACCEPTANCE);
    assert(i2c_probe_exchange_status(&exchange, 0x17));
    assert(exchange.stage == I2C_PROBE_EXCHANGE_WAIT_ACCEPTANCE);
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(exchange.stage == I2C_PROBE_EXCHANGE_WAIT_RESPONSE);
    assert(i2c_probe_exchange_finalize(&exchange, &response, true));
    assert(exchange.stage == I2C_PROBE_EXCHANGE_COMPLETE);
    assert(exchange.result == I2C_PROBE_EXCHANGE_SUCCEEDED);
}

static void test_limits_unexpected_ready_responses(void) {
    I2cProbeExchange exchange;

    i2c_probe_exchange_init(&exchange);
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        assert(i2c_probe_exchange_status(&exchange, 0x41));
        assert(exchange.stage == I2C_PROBE_EXCHANGE_WAIT_READY);
    }
    assert(i2c_probe_exchange_status(&exchange, 0x41));
    assert(exchange.stage == I2C_PROBE_EXCHANGE_FAILED);
    assert(exchange.result == I2C_PROBE_EXCHANGE_COMMAND_ERROR);

    i2c_probe_exchange_init(&exchange);
    for (uint8_t attempt = 0; attempt < 8; ++attempt) {
        assert(i2c_probe_exchange_status(&exchange, 0x17));
        assert(exchange.stage == I2C_PROBE_EXCHANGE_WAIT_READY);
    }
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(exchange.readiness.retry_count == 0);
}

static void test_rejects_command_acceptance(void) {
    I2cProbeExchange exchange;

    i2c_probe_exchange_init(&exchange);
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(i2c_probe_exchange_command_queued(&exchange));
    assert(i2c_probe_exchange_status(&exchange, 0x06));
    assert(exchange.stage == I2C_PROBE_EXCHANGE_FAILED);
    assert(exchange.result == I2C_PROBE_EXCHANGE_COMMAND_ERROR);
}

static void test_classifies_final_response_errors(void) {
    static const uint8_t payload[] = {0x12, 0x34};
    I2cProbeFinalResponse response = {
        .payload = payload,
        .payload_length = sizeof(payload),
        .expected_checksum = 0,
    };
    I2cProbeExchange exchange;

    i2c_probe_exchange_init(&exchange);
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(i2c_probe_exchange_command_queued(&exchange));
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(i2c_probe_exchange_finalize(&exchange, &response, true));
    assert(exchange.result == I2C_PROBE_EXCHANGE_CHECKSUM_ERROR);

    i2c_probe_exchange_init(&exchange);
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(i2c_probe_exchange_command_queued(&exchange));
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    response.primary_status = 1;
    assert(i2c_probe_exchange_finalize(&exchange, &response, false));
    assert(exchange.result == I2C_PROBE_EXCHANGE_RESPONSE_ERROR);
}

static void test_rejects_out_of_order_exchange_events(void) {
    I2cProbeExchange exchange;
    I2cProbeFinalResponse response = {0};

    i2c_probe_exchange_init(&exchange);
    assert(!i2c_probe_exchange_command_queued(&exchange));
    assert(!i2c_probe_exchange_finalize(&exchange, &response, false));
    assert(i2c_probe_exchange_status(&exchange, 0x07));
    assert(!i2c_probe_exchange_status(&exchange, 0x07));
}

int main(void) {
    test_accepts_handshake_and_clears_retry_count();
    test_busy_handshake_increments_without_rejection();
    test_rejects_fourth_unexpected_handshake_response();
    test_classifies_command_responses();
    test_calculates_xor_checksum();
    test_rejects_nonzero_final_status_before_checksum();
    test_optionally_validates_final_checksum();
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
    test_completes_probe_exchange();
    test_limits_unexpected_ready_responses();
    test_rejects_command_acceptance();
    test_classifies_final_response_errors();
    test_rejects_out_of_order_exchange_events();
    return 0;
}
