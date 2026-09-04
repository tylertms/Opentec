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

static void test_completes_callback_on_endpoint(uint8_t report_id) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);

    assert(wheel_protocol_bridge_service_request(&service, report_id));
    assert(service.report_id == report_id);
    uint8_t refreshed_report_id =
        report_id == WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD
            ? WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED
            : WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD;
    assert(wheel_protocol_bridge_service_request(&service, refreshed_report_id));
    assert(service.report_id == refreshed_report_id);
    wheel_protocol_bridge_service_run(&service);
    assert(transport.owner == refreshed_report_id);
    assert_request(&transport, refreshed_report_id);
    assert(wheel_protocol_bridge_service_request(&service, report_id));
    assert(service.report_id == refreshed_report_id);
    const uint8_t accepted[] = {1};
    command_transport_receive(&transport, accepted, sizeof(accepted));
    wheel_protocol_bridge_service_run(&service);
    assert(wheel_protocol_bridge_service_take_acknowledgement(&service));
    assert(!wheel_protocol_bridge_service_take_acknowledgement(&service));
    assert(transport.owner == 0);
}

static void test_recovers_after_rejected_transfer(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);

    assert(
        wheel_protocol_bridge_service_request(&service, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD));
    wheel_protocol_bridge_service_run(&service);
    assert_request(&transport, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD);
    command_transport_fail(&transport);
    wheel_protocol_bridge_service_run(&service);
    assert(!wheel_protocol_bridge_service_take_acknowledgement(&service));
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_ERROR);
    assert(transport.owner == 0);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_STARTUP_RECOVERY);
    assert(transport.owner == 0);
    assert(transport.completion == COMMAND_TRANSPORT_COMPLETE);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_READY);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING);
    assert_request(&transport, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD);
}

static void test_waits_for_shared_transport(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);
    command_transport_claim(&transport, 0x20);

    assert(
        wheel_protocol_bridge_service_request(&service, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED));
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_READY);
    command_transport_release(&transport, 0x20);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING);
    assert(transport.owner == WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED);
}

static void test_requires_nonzero_report_identifier(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);

    assert(!wheel_protocol_bridge_service_request(&service, 0));
    assert(wheel_protocol_bridge_service_request(&service, 0x14));
    assert(service.report_id == 0x14);
    assert(wheel_protocol_bridge_service_request(&service, 0x17));
    assert(service.report_id == 0x17);
}

static void test_refreshes_target_without_clearing_acknowledgement(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);

    service.acknowledged = true;
    assert(wheel_protocol_bridge_service_request(&service,
                                                WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD));
    assert(service.acknowledged);
    assert(wheel_protocol_bridge_service_request(&service,
                                                WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED));
    assert(service.report_id == WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED);
    assert(service.acknowledged);
}

static void test_recovers_after_the_wait_limit(void) {
    WheelProtocolBridgeService service;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_protocol_bridge_service_init(&service, &transport);

    assert(wheel_protocol_bridge_service_request(
        &service, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD));
    wheel_protocol_bridge_service_run(&service);
    assert_request(&transport, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD);
    for (uint16_t poll = 0; poll < 500; ++poll) {
        wheel_protocol_bridge_service_run(&service);
        assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING);
    }
    assert(service.wait_calls == 500);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_ERROR);
    assert(transport.phase == COMMAND_TRANSPORT_IDLE);
    assert(transport.owner == WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD);
    assert(transport.completion == COMMAND_TRANSPORT_WRITE_REJECTED);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_STARTUP_RECOVERY);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_READY);
    wheel_protocol_bridge_service_run(&service);
    assert(service.phase == WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING);
    assert_request(&transport, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD);
    assert(transport.owner == WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD);
}

static void test_rejects_unavailable_service(void) {
    WheelProtocolBridgeService service;
    wheel_protocol_bridge_service_init(&service, NULL);
    assert(!wheel_protocol_bridge_service_request(NULL, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD));
    assert(
        !wheel_protocol_bridge_service_request(&service, WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD));
    assert(!wheel_protocol_bridge_service_take_acknowledgement(NULL));
    wheel_protocol_bridge_service_run(NULL);
}

int main(void) {
    test_completes_callback_on_endpoint(WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD);
    test_completes_callback_on_endpoint(WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED);
    test_recovers_after_rejected_transfer();
    test_waits_for_shared_transport();
    test_requires_nonzero_report_identifier();
    test_refreshes_target_without_clearing_acknowledgement();
    test_recovers_after_the_wait_limit();
    test_rejects_unavailable_service();
    return 0;
}
