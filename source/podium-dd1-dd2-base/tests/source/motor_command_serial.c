#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_serial.h"
#include "platform/serial_link.h"
#include "serial/packet.h"
#include "serial/service.h"
#include "transfer/command.h"

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

static void queue_response(const uint8_t *payload, uint8_t length) {
    assert(serial_packet_encode(4, 1, payload, length, received));
    received_ready = true;
}

static void test_submits_write_and_applies_response(void) {
    CommandTransport transport;
    SerialService service;
    static const uint8_t data[] = {0xaa, 0xbb};
    static const uint8_t expected[] = {2, 0x40, 0x90, 0xaa, 0xbb};
    command_transport_init(&transport);
    serial_service_init(&service);

    assert(command_transport_queue_write(&transport, 0x20, 0x90, data, sizeof(data)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(motor_command_serial_submit(&transport, &service, 0));
    SerialPacket packet;
    assert(serial_packet_decode(transmitted, &packet) == SERIAL_PACKET_VALID);
    assert(packet.type_flags == 4);
    assert(packet.sequence == 0);
    assert(packet.payload_length == sizeof(expected));
    assert(memcmp(packet.payload, expected, sizeof(expected)) == 0);

    static const uint8_t accepted[] = {1};
    queue_response(accepted, sizeof(accepted));
    serial_service_run(&service, 1);
    assert(motor_command_serial_receive(&transport, &service));
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_COMPLETE);
    assert(service.status == SERIAL_SERVICE_IDLE);
}

static void test_submits_read_and_copies_response(void) {
    CommandTransport transport;
    SerialService service;
    uint8_t output[3] = {0};
    command_transport_init(&transport);
    serial_service_init(&service);

    assert(command_transport_queue_read(&transport, 0x20, 0x80, output, sizeof(output)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(motor_command_serial_submit(&transport, &service, 0));
    static const uint8_t response[] = {1, 0, 0x11, 0x22, 0x33};
    queue_response(response, sizeof(response));
    serial_service_run(&service, 1);
    assert(motor_command_serial_receive(&transport, &service));
    assert(memcmp(output, response + 2, sizeof(output)) == 0);
}

static void test_ignores_other_message_types(void) {
    CommandTransport transport;
    SerialService service;
    static const uint8_t payload[] = {1};
    command_transport_init(&transport);
    serial_service_init(&service);
    assert(serial_service_start(&service, 2, payload, sizeof(payload), 0));
    assert(!motor_command_serial_receive(&transport, &service));
    assert(service.status == SERIAL_SERVICE_PENDING);
}

static void test_fails_command_after_serial_attempts(void) {
    CommandTransport transport;
    SerialService service;
    command_transport_init(&transport);
    serial_service_init(&service);
    assert(command_transport_queue_read(&transport, 0x20, 0, 0, 0) == COMMAND_TRANSPORT_COMPLETE);
    assert(motor_command_serial_submit(&transport, &service, 0));
    service.status = SERIAL_SERVICE_FAILED;

    assert(motor_command_serial_receive(&transport, &service));
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_READ_REJECTED);
    assert(service.status == SERIAL_SERVICE_IDLE);
}

int main(void) {
    test_submits_write_and_applies_response();
    test_submits_read_and_copies_response();
    test_ignores_other_message_types();
    test_fails_command_after_serial_attempts();
    return 0;
}
