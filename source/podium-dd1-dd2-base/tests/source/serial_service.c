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

void platform_serial_link_init(void) {}

void platform_serial_link_reset(void) { reset_count++; }

bool platform_serial_link_start(const uint8_t packet[SERIAL_PACKET_SIZE]) {
    memcpy(transmitted, packet, sizeof(transmitted));
    start_count++;
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

static void reset_link(void) {
    memset(transmitted, 0, sizeof(transmitted));
    memset(received, 0, sizeof(received));
    start_count = 0;
    reset_count = 0;
    received_ready = false;
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

static void test_counts_invalid_packets(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 3, &data, 1, 0));

    received[0] = 0;
    received_ready = true;
    serial_service_run(&service, 1);
    assert(serial_service_error_count(&service) == 1);
}

static void test_retries_four_times_after_initial_send(void) {
    SerialService service;
    reset_link();
    serial_service_init(&service);
    const uint8_t data = 1;
    assert(serial_service_start(&service, 3, &data, 1, 100));

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

int main(void) {
    test_completes_matching_transaction();
    test_rejects_overlapping_transaction();
    test_fails_mismatched_response();
    test_counts_invalid_packets();
    test_retries_four_times_after_initial_send();
    return 0;
}
