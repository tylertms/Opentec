#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/serial_link.h"
#include "serial/packet.h"
#include "serial/service.h"

static uint8_t transmitted[SERIAL_PACKET_SIZE];
static uint8_t received[SERIAL_PACKET_SIZE];
static uint8_t start_count;
static uint8_t reset_count;
static bool received_ready;
static bool link_active;
static bool recovery_active;
static bool fail_start;

void platform_serial_link_init(void) {}

void platform_serial_link_reset(void) {
    reset_count++;
    link_active = false;
}

bool platform_serial_link_start_periodic_recovery(void) {
    recovery_active = true;
    link_active = false;
    return true;
}

bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]) {
    if (link_active || recovery_active || fail_start) {
        return false;
    }
    memcpy(transmitted, packet, sizeof(transmitted));
    start_count++;
    link_active = true;
    return true;
}

bool platform_serial_link_take_received(uint8_t packet[SERIAL_PACKET_SIZE]) {
    if (!received_ready) {
        return false;
    }
    memcpy(packet, received, sizeof(received));
    received_ready = false;
    link_active = false;
    return true;
}

static void reset_link(void) {
    memset(transmitted, 0, sizeof(transmitted));
    memset(received, 0, sizeof(received));
    start_count = 0;
    reset_count = 0;
    received_ready = false;
    link_active = false;
    recovery_active = false;
    fail_start = false;
}

static void finish_recovery(void) {
    recovery_active = false;
}

static SerialPacket transmitted_packet(void) {
    SerialPacket packet;
    assert(serial_packet_decode(transmitted, &packet) == SERIAL_PACKET_VALID);
    return packet;
}

static void queue_response(uint8_t type, uint8_t sequence, const uint8_t *message, uint8_t length) {
    assert(serial_packet_encode(type, sequence, message, length, received));
    received_ready = true;
}

static void queue_malformed_response(void) {
    memset(received, 0, sizeof(received));
    received_ready = true;
}

static void queue_oversized_unsupported_response(void) {
    memset(received, 0, sizeof(received));
    received[0] = SERIAL_PACKET_START;
    received[1] = 6;
    received[3] = SERIAL_PACKET_MAX_PAYLOAD_SIZE + 1;
    received[61] = 0xfb;
    received[62] = 0x5e;
    received[SERIAL_PACKET_SIZE - 1] = SERIAL_PACKET_END;
    assert(serial_packet_checksum_valid(received));
    received_ready = true;
}

static void test_completes_matching_transaction(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t request_data[3] = {1, 2, 3};

    assert(serial_service_start(&service, 3, request_data, sizeof(request_data), 100));
    assert(start_count == 1);
    SerialPacket request = transmitted_packet();
    assert(request.type_flags == 3);
    assert(request.sequence == 0);
    assert(request.payload_length == sizeof(request_data));
    assert(memcmp(request.payload, request_data, sizeof(request_data)) == 0);

    const uint8_t response_data[2] = {9, 8};
    queue_response(3, 0, response_data, sizeof(response_data));
    serial_service_run(&service, 101);

    const SerialMessageAssembly *response = serial_service_response(&service);
    assert(service.status == SERIAL_SERVICE_SUCCEEDED);
    assert(response != 0);
    assert(response->length == sizeof(response_data));
    assert(memcmp(response->data, response_data, sizeof(response_data)) == 0);

    serial_service_release(&service);
    assert(serial_service_start(&service, 4, request_data, sizeof(request_data), 102));
    assert(transmitted_packet().sequence == 1);
}

static void test_rejects_overlapping_transaction(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;

    assert(serial_service_start(&service, 3, &data, 1, 0));
    assert(!serial_service_start(&service, 5, &data, 1, 1));
    assert(start_count == 1);
}

static void test_enforces_logical_message_limit(void) {
    SerialService service;
    static uint8_t message[SERIAL_MESSAGE_MAX_SIZE + 1];
    reset_link();
    serial_service_init(&service);

    assert(serial_service_start(&service, 3, message, SERIAL_MESSAGE_MAX_SIZE, 0));
    serial_service_cancel(&service);
    assert(!serial_service_start(&service, 3, message, sizeof(message), 1));
}

static void test_fails_mismatched_response(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 3, &data, 1, 0));

    queue_response(5, 0, &data, 1);
    serial_service_run(&service, 1);
    assert(service.status == SERIAL_SERVICE_FAILED);
    assert(serial_service_response(&service) == 0);
}

static void test_ignores_unsupported_framed_commands(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 3, &data, 1, 0));

    queue_response(6, 0, &data, 1);
    serial_service_run(&service, 1);
    assert(service.status == SERIAL_SERVICE_PENDING);
    assert(service.attempts == 0);
    assert(serial_service_error_count(&service) == 0);
    assert(start_count == 2);
    assert(transmitted_packet().type_flags == 0);
}

static void test_ignores_oversized_unsupported_framed_commands(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 3, &data, 1, 0));

    queue_oversized_unsupported_response();
    serial_service_run(&service, 1);
    assert(service.status == SERIAL_SERVICE_PENDING);
    assert(service.attempts == 0);
    assert(serial_service_error_count(&service) == 0);
    assert(start_count == 2);
    assert(transmitted_packet().type_flags == 0);
}

static void test_does_not_send_sync_after_bounded_framing_failure(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start_wait(&service, 3, &data, 1, 100));

    for (uint8_t attempt = 0; attempt < 5; attempt++) {
        queue_malformed_response();
        serial_service_run(&service, (uint32_t)attempt + 1u);
        if (attempt < 4) {
            assert(service.recovery_pending);
            finish_recovery();
            serial_service_run(&service, (uint32_t)attempt + 2u);
        }
    }

    assert(service.status == SERIAL_SERVICE_FAILED);
    assert(service.attempts == 5);
    assert(service.packet_pending == false);
    assert(service.recovery_pending == false);
    assert(start_count == 5);
    assert(recovery_active);
    assert(transmitted_packet().type_flags == 0);
    assert(serial_service_error_count(&service) == 5);
}

static void test_truncated_response_waits_for_recovery_before_sync(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 3, &data, 1, 100));

    queue_malformed_response();
    serial_service_run(&service, 101);
    assert(service.recovery_pending);
    assert(start_count == 1);
    serial_service_run(&service, 102);
    assert(start_count == 1);

    finish_recovery();
    serial_service_run(&service, 103);
    assert(!service.recovery_pending);
    assert(service.packet_pending);
    assert(start_count == 2);
    assert(transmitted_packet().type_flags == 0);
}

static void test_checksum_failure_does_not_send_fifth_sync(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start_wait(&service, 3, &data, 1, 100));

    for (uint8_t attempt = 0; attempt < 5; attempt++) {
        assert(serial_packet_encode(3, 0, &data, 1, received));
        received[61] ^= 1;
        received_ready = true;
        serial_service_run(&service, (uint32_t)attempt + 1u);
        if (attempt < 4) {
            assert(service.status == SERIAL_SERVICE_PENDING);
            assert(service.packet_pending);
            assert(start_count == (uint8_t)attempt + 2u);
        }
    }

    assert(service.status == SERIAL_SERVICE_FAILED);
    assert(service.attempts == 5);
    assert(!service.packet_pending);
    assert(!service.recovery_pending);
    assert(!recovery_active);
    assert(start_count == 5);
    assert(transmitted_packet().type_flags == 0);
    assert(serial_service_error_count(&service) == 5);
}

static void test_retries_four_times_after_initial_send(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start_wait(&service, 3, &data, 1, 100));

    serial_service_run(&service, 109);
    assert(service.status == SERIAL_SERVICE_PENDING);
    serial_service_run(&service, 110);
    assert(service.status == SERIAL_SERVICE_PENDING);
    assert(reset_count == 0);
    serial_service_run(&service, 111);
    assert(service.status == SERIAL_SERVICE_PENDING);
    assert(reset_count == 1);
    assert(start_count == 2);
    serial_service_run(&service, 122);
    assert(service.status == SERIAL_SERVICE_PENDING);
    serial_service_run(&service, 133);
    assert(service.status == SERIAL_SERVICE_PENDING);
    serial_service_run(&service, 144);
    assert(service.status == SERIAL_SERVICE_PENDING);
    serial_service_run(&service, 155);
    assert(service.status == SERIAL_SERVICE_FAILED);
    assert(reset_count == 5);
    assert(start_count == 5);
    assert(serial_service_error_count(&service) == 5);
}

static void test_failed_restart_reaches_bounded_failure(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start_wait(&service, 3, &data, 1, 100));
    fail_start = true;

    for (uint8_t attempt = 0; attempt < 5; attempt++) {
        serial_service_run(&service, (uint32_t)attempt + 111u);
        if (attempt < 4) {
            assert(service.status == SERIAL_SERVICE_PENDING);
            assert(service.packet_pending);
        }
    }

    assert(service.status == SERIAL_SERVICE_FAILED);
    assert(!service.packet_pending);
    assert(service.attempts == 5);
    assert(reset_count == 5);
    assert(start_count == 1);
    assert(serial_service_error_count(&service) == 5);
}

static void test_start_retries_without_bound(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 3, &data, 1, 100));

    serial_service_run(&service, 111);
    serial_service_run(&service, 122);
    serial_service_run(&service, 133);
    serial_service_run(&service, 144);
    serial_service_run(&service, 155);

    assert(service.status == SERIAL_SERVICE_PENDING);
    assert(service.attempts == 5);
    assert(reset_count == 5);
    assert(start_count == 6);
    assert(serial_service_error_count(&service) == 5);
}

static void test_cancels_pending_transaction_without_resetting_sequence(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 4, &data, 1, 0));
    uint8_t sequence = service.session.sequence;

    serial_service_cancel(&service);
    assert(service.status == SERIAL_SERVICE_IDLE);
    assert(service.request_type == 0);
    assert(reset_count == 1);
    assert(serial_service_start(&service, 2, &data, 1, 1));
    assert(transmitted_packet().sequence == sequence);
}

int main(void) {
    test_completes_matching_transaction();
    test_rejects_overlapping_transaction();
    test_enforces_logical_message_limit();
    test_fails_mismatched_response();
    test_ignores_unsupported_framed_commands();
    test_ignores_oversized_unsupported_framed_commands();
    test_does_not_send_sync_after_bounded_framing_failure();
    test_truncated_response_waits_for_recovery_before_sync();
    test_checksum_failure_does_not_send_fifth_sync();
    test_retries_four_times_after_initial_send();
    test_failed_restart_reaches_bounded_failure();
    test_start_retries_without_bound();
    test_cancels_pending_transaction_without_resetting_sequence();
    return 0;
}
