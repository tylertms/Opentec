#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/protocol_bridge_service.h"

static void assert_request(CommandTransport *transport, uint8_t target) {
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(transport, &request, &length));
    const uint8_t expected[] = {2, (uint8_t)(target << 1), 0x0d, 0xfa, 0x05};
    assert(length == sizeof(expected));
    assert(memcmp(request, expected, sizeof(expected)) == 0);
    assert(command_transport_request_sent(transport));
}

static void test_completes_callback_on_standard_endpoint(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);

    assert(wheel_protocol_bridge_service_request(&service));
    assert(!wheel_protocol_bridge_service_request(&service));
    wheel_protocol_bridge_service_run(&service);
    assert_request(&transport, 0x15);
    const uint8_t accepted[] = {1};
    command_transport_receive(&transport, accepted, sizeof(accepted));
    wheel_protocol_bridge_service_run(&service);
    assert(wheel_protocol_bridge_service_take_acknowledgement(&service));
    assert(!wheel_protocol_bridge_service_take_acknowledgement(&service));
    assert(transport.owner == 0);
}

static void test_retries_extended_endpoint_after_rejection(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);

    assert(wheel_protocol_bridge_service_request(&service));
    wheel_protocol_bridge_service_run(&service);
    assert_request(&transport, 0x15);
    command_transport_fail(&transport);
    wheel_protocol_bridge_service_run(&service);
    wheel_protocol_bridge_service_run(&service);
    assert_request(&transport, 0x16);
    command_transport_fail(&transport);
    wheel_protocol_bridge_service_run(&service);
    assert(!wheel_protocol_bridge_service_take_acknowledgement(&service));
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_IDLE);
    assert(transport.owner == 0);
}

static void test_waits_for_shared_transport(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);
    command_transport_claim(&transport, 0x20);

    assert(wheel_protocol_bridge_service_request(&service));
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_READY);
    command_transport_release(&transport, 0x20);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING);
}

static void test_rejects_unavailable_service(void) {
    WheelProtocolBridgeService service;
    wheel_protocol_bridge_service_init(&service, NULL);
    assert(!wheel_protocol_bridge_service_request(NULL));
    assert(!wheel_protocol_bridge_service_request(&service));
    assert(!wheel_protocol_bridge_service_take_acknowledgement(NULL));
    wheel_protocol_bridge_service_run(NULL);
}

int main(void) {
    test_completes_callback_on_standard_endpoint();
    test_retries_extended_endpoint_after_rejection();
    test_waits_for_shared_transport();
    test_rejects_unavailable_service();
    return 0;
}
