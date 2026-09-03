#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/serial_link.h"
#include "serial/packet.h"
#include "serial/service.h"
#include "wheel/status_service.h"

static uint8_t transmitted[SERIAL_PACKET_SIZE];
static uint8_t received[SERIAL_PACKET_SIZE];
static bool received_ready;

void platform_serial_link_init(void) {}

void platform_serial_link_reset(void) {}

bool platform_serial_link_start_periodic_recovery(void) { return true; }

bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]) {
    memcpy(transmitted, packet, sizeof(transmitted));
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
    serial_service_init(transport);
    wheel_status_service_init(service, transport);
}

static void test_polls_and_decodes_status(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    wheel_status_service_run(&service, 0, true);
    SerialPacket packet = request();
    assert(packet.type_flags == 5);
    assert(packet.payload_length == 1);
    assert(packet.payload[0] == 0);

    static const uint8_t response[15] = {0x12, 0x34, 0x78, 0x56, 0x04, 0x03, 0x02, 0x01,
                                         0x0d, 0x0c, 0x0b, 0x0a, 0x9a, 0xff, 0};
    respond(packet.sequence, response);
    serial_service_run(&transport, 1);
    wheel_status_service_run(&service, 1, true);

    const WheelStatusSnapshot *snapshot = wheel_status_service_snapshot(&service);
    assert(snapshot->status_high == 0x12);
    assert(snapshot->status_low == 0x34);
    assert(snapshot->accessory_value == 0x5678);
    assert(snapshot->runtime_seconds == UINT32_C(0x01020304));
    assert(snapshot->runtime_counter == UINT32_C(0x0a0b0c0d));
    assert(snapshot->trailing_status == 0x9a);
    assert(transport.status == SERIAL_SERVICE_IDLE);
}

static void test_enforces_poll_interval(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    wheel_status_service_run(&service, 10, true);
    SerialPacket first = request();
    static const uint8_t response[15] = {0};
    respond(first.sequence, response);
    serial_service_run(&transport, 11);
    wheel_status_service_run(&service, 11, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);

    wheel_status_service_run(&service, 1009, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    wheel_status_service_run(&service, 1010, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
    assert(request().sequence == 1);
}

static void test_marks_and_takes_transition_response(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    wheel_status_service_mark_next_request(&service);
    wheel_status_service_run(&service, 20, true);
    SerialPacket packet = request();
    assert(packet.payload[0] == 0xaa);
    static const uint8_t response[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xaa};
    respond(packet.sequence, response);
    serial_service_run(&transport, 21);
    wheel_status_service_run(&service, 21, true);

    assert(wheel_status_service_take_marked_response(&service));
    assert(!wheel_status_service_take_marked_response(&service));
}

static void test_marked_transition_bypasses_poll_deadline(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    wheel_status_service_run(&service, 10, true);
    SerialPacket first = request();
    static const uint8_t response[15] = {0};
    respond(first.sequence, response);
    serial_service_run(&transport, 11);
    wheel_status_service_run(&service, 11, true);
    assert(transport.status == SERIAL_SERVICE_IDLE);

    wheel_status_service_mark_next_request(&service);
    wheel_status_service_run(&service, 12, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
    SerialPacket marked = request();
    assert(marked.payload[0] == 0xaa);
}

static void test_waits_for_scheduler_slot(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    wheel_status_service_run(&service, 0, false);
    assert(transport.status == SERIAL_SERVICE_IDLE);
    wheel_status_service_run(&service, 1, true);
    assert(transport.status == SERIAL_SERVICE_PENDING);
}

static void test_reports_active_exchange_until_release(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    assert(!wheel_status_service_exchange_active(&service));
    wheel_status_service_run(&service, 0, true);
    assert(wheel_status_service_exchange_active(&service));
    transport.status = SERIAL_SERVICE_SUCCEEDED;
    assert(wheel_status_service_exchange_active(&service));
    wheel_status_service_run(&service, 1, false);
    assert(!wheel_status_service_exchange_active(&service));
}

static void test_status_request_stops_after_fifth_timeout(void) {
    WheelStatusService service;
    SerialService transport;
    initialize(&service, &transport);

    wheel_status_service_run(&service, 0, true);
    for (uint32_t attempt = 0; attempt < 5; attempt++) {
        serial_service_run(&transport, 11u + attempt * 11u);
    }

    assert(transport.status == SERIAL_SERVICE_FAILED);
    assert(transport.attempts == 5);
    wheel_status_service_run(&service, 56, false);
    assert(transport.status == SERIAL_SERVICE_IDLE);
}

int main(void) {
    test_polls_and_decodes_status();
    test_enforces_poll_interval();
    test_marks_and_takes_transition_response();
    test_marked_transition_bypasses_poll_deadline();
    test_waits_for_scheduler_slot();
    test_reports_active_exchange_until_release();
    test_status_request_stops_after_fifth_timeout();
    return 0;
}
