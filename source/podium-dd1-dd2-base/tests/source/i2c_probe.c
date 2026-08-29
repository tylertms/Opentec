#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

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
    return 0;
}
