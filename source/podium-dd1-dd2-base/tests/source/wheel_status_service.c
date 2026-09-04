#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/serial_link.h"
#include "platform/time.h"
#include "serial/packet.h"
#include "serial/service.h"
#include "wheel/status_service.h"

static uint8_t transmitted[SERIAL_PACKET_SIZE];
static uint8_t received[SERIAL_PACKET_SIZE];
static bool received_ready;
static uint32_t current_time_ms;
static uint32_t start_time_advance_ms;

uint32_t platform_time_ms(void) { return current_time_ms; }

void platform_serial_link_init(void) {}

void platform_serial_link_reset(void) {}

bool platform_serial_link_start_periodic_recovery(void) { return true; }

bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]) {
    memcpy(transmitted, packet, sizeof(transmitted));
    current_time_ms += start_time_advance_ms;
    return true;
}

bool platform_serial_link_take_received(uint8_t packet[SERIAL_PACKET_SIZE]) {
    if (!received_ready) {
        return false;
    }
    memcpy(packet, received, sizeof(received));
    received_ready = false;
    return true;
}

static SerialPacket request(void) {
    SerialPacket packet;
    assert(serial_packet_decode(transmitted, &packet) == SERIAL_PACKET_VALID);
    return packet;
}

static void respond(uint8_t sequence, const uint8_t payload[15]) {
    assert(serial_packet_encode(5, sequence, payload, 15, received));
    received_ready = true;
}

static void initialize(WheelStatusService *service, SerialService *transport) {
    memset(transmitted, 0, sizeof(transmitted));
    memset(received, 0, sizeof(received));
    received_ready = false;
    current_time_ms = 0;
    start_time_advance_ms = 0;
    serial_service_init(transport);
    wheel_status_service_init(service, transport);
}

static void run(WheelStatusService *service, uint32_t now_ms, bool start_allowed) {
    current_time_ms = now_ms;
    wheel_status_service_run(service, start_allowed);
}

static void test_polls_and_decodes_status(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    run(&service, 0, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    run(&service, 1, true);
    SerialPacket packet = request();
    assert(packet.type_flags == 5);
    assert(packet.payload_length == 1);
    assert(packet.payload[0] == 0);

    static const uint8_t response[15] = {0x12, 0x34, 0x78, 0x56, 0x04, 0x03, 0x02, 0x01,
                                         0x0d, 0x0c, 0x0b, 0x0a, 0x9a, 0xff, 0};
    respond(packet.sequence, response);
    serial_service_run(&transport, 2);
    run(&service, 2, true);

    const WheelStatusSnapshot *snapshot = wheel_status_service_snapshot(&service);
    assert(snapshot->status_high == 0x12);
    assert(snapshot->status_low == 0x34);
    assert(snapshot->accessory_value == 0x5678);
    assert(snapshot->runtime_seconds == UINT32_C(0x01020304));
    assert(snapshot->runtime_counter == UINT32_C(0x0a0b0c0d));
    assert(snapshot->trailing_status == 0x9a);
    assert(transport.status == SERIAL_SERVICE_IDLE);
}

static void test_runs_startup_transaction_to_completion(void) {
    WheelStatusService service;
    SerialService transport;
    WheelStatusStartupTransaction transaction;
    initialize(&service, &transport);

    wheel_status_service_mark_next_request(&service);
    wheel_status_startup_transaction_init(&transaction, &service);
    current_time_ms = 301;
    start_time_advance_ms = 10;
    assert(wheel_status_startup_transaction_run(&transaction, 301) == WHEEL_STATUS_STARTUP_WAIT);
    start_time_advance_ms = 0;
    SerialPacket packet = request();
    assert(packet.type_flags == 5);
    assert(packet.payload_length == 1);
    assert(packet.payload[0] == 0);
    assert(service.next_poll_ms == 1311);

    static const uint8_t response[15] = {0x12, 0x34, 0x78, 0x56, 0x04, 0x03, 0x02, 0x01,
                                         0x0d, 0x0c, 0x0b, 0x0a, 0x9a, 0xff, 0};
    respond(packet.sequence, response);
    serial_service_run(&transport, 302);
    assert(wheel_status_startup_transaction_run(&transaction, 302) ==
           WHEEL_STATUS_STARTUP_COMPLETE);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    assert(wheel_status_service_snapshot(&service)->status_high == 0x12);
}

static void test_reports_terminal_failure_after_fifth_startup_retry(void) {
    WheelStatusService service;
    SerialService transport;
    WheelStatusStartupTransaction transaction;
    initialize(&service, &transport);

    wheel_status_startup_transaction_init(&transaction, &service);
    assert(WHEEL_STATUS_STARTUP_FAILED == 3);
    assert(wheel_status_startup_transaction_run(&transaction, 1) == WHEEL_STATUS_STARTUP_WAIT);
    for (uint32_t attempt = 0; attempt < 5; ++attempt) {
        uint32_t now_ms = 12u + attempt * 11u;
        serial_service_run(&transport, now_ms);
        if (attempt < 4) {
            assert(wheel_status_startup_transaction_run(&transaction, now_ms) ==
                   WHEEL_STATUS_STARTUP_WAIT);
        } else {
            assert(wheel_status_startup_transaction_run(&transaction, now_ms) ==
                   WHEEL_STATUS_STARTUP_FAILED);
        }
    }
    assert(transport.status == SERIAL_SERVICE_IDLE);
}

static void test_enforces_strict_poll_deadline(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    run(&service, 10, true);
    assert(service.next_poll_ms == 1010);
    SerialPacket first = request();
    static const uint8_t response[15] = {0};
    respond(first.sequence, response);
    serial_service_run(&transport, 11);
    run(&service, 11, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);

    run(&service, 1009, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    run(&service, 1010, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    run(&service, 1011, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
    assert(request().sequence == 1);
}

static void test_marks_and_takes_transition_response(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    wheel_status_service_mark_next_request(&service);
    run(&service, 20, true);
    SerialPacket packet = request();
    assert(packet.payload[0] == 0xaa);
    static const uint8_t response[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xaa};
    respond(packet.sequence, response);
    serial_service_run(&transport, 21);
    run(&service, 21, true);

    assert(wheel_status_service_take_marked_response(&service));
    assert(!wheel_status_service_take_marked_response(&service));
}

static void test_marked_transition_preserves_poll_deadline(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    run(&service, 10, true);
    SerialPacket first = request();
    static const uint8_t response[15] = {0};
    respond(first.sequence, response);
    serial_service_run(&transport, 11);
    run(&service, 11, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);

    wheel_status_service_mark_next_request(&service);
    assert(service.next_poll_ms == 1010);
    run(&service, 12, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    run(&service, 1011, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
    SerialPacket marked = request();
    assert(marked.payload[0] == 0xaa);
}

static void test_uses_official_unsigned_deadline_ordering(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    service.next_poll_ms = UINT32_MAX - 10u;
    run(&service, 5, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    run(&service, UINT32_MAX - 9u, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
}

static void test_records_deadline_after_request_starts(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    start_time_advance_ms = 10;
    run(&service, 10, true);
    start_time_advance_ms = 0;
    assert(service.next_poll_ms == 1020);
}

static void test_waits_for_scheduler_slot(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    run(&service, 0, false);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    run(&service, 1, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
}

static void test_reports_active_exchange_until_release(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    assert(!wheel_status_service_exchange_active(&service));
    run(&service, 1, true);
    assert(wheel_status_service_exchange_active(&service));
    transport.status = SERIAL_SERVICE_SUCCEEDED;
    assert(wheel_status_service_exchange_active(&service));
    run(&service, 2, false);
    assert(!wheel_status_service_exchange_active(&service));
}

static void test_status_request_stops_after_fifth_timeout(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    run(&service, 1, true);
    for (uint32_t attempt = 0; attempt < 5; attempt++) {
        serial_service_run(&transport, 12u + attempt * 11u);
    }

    assert(transport.status == SERIAL_SERVICE_FAILED);
    assert(transport.attempts == 5);
    run(&service, 56, false);
    assert(transport.status == SERIAL_SERVICE_IDLE);
}

int main(void) {
    test_polls_and_decodes_status();
    test_runs_startup_transaction_to_completion();
    test_reports_terminal_failure_after_fifth_startup_retry();
    test_enforces_strict_poll_deadline();
    test_marks_and_takes_transition_response();
    test_marked_transition_preserves_poll_deadline();
    test_uses_official_unsigned_deadline_ordering();
    test_records_deadline_after_request_starts();
    test_waits_for_scheduler_slot();
    test_reports_active_exchange_until_release();
    test_status_request_stops_after_fifth_timeout();
    return 0;
}
