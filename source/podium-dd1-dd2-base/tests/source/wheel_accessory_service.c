#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/accessory.h"
#include "wheel/accessory_service.h"

static int8_t signed_status(uint8_t value) { return (int8_t)value; }

static void submit_request(CommandTransport *transport, const uint8_t *expected,
                           uint16_t expected_length) {
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(transport, &request, &length));
    assert(length == expected_length);
    assert(memcmp(request, expected, length) == 0);
    assert(command_transport_request_sent(transport));
}

static void complete_read(CommandTransport *transport, const uint8_t *data, uint8_t length) {
    uint8_t response[6] = {1, 0};
    memcpy(response + 2, data, length);
    command_transport_receive(transport, response, (uint16_t)length + 2);
}

static void polls_status_then_version_and_applies_identity(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);

    wheel_accessory_service_run(&service, &transport);
    const uint8_t expected_status[] = {2, 0xe1, 0, 1, 0};
    submit_request(&transport, expected_status, sizeof(expected_status));
    const uint8_t status[] = {0x8a};
    complete_read(&transport, status, sizeof(status));
    wheel_accessory_service_run(&service, &transport);
    assert(service.version_stage);
    assert(!service.request_pending);
    assert(transport.owner == 0);

    wheel_accessory_service_run(&service, &transport);
    const uint8_t expected_version[] = {2, 0xe1, 1, 4, 0};
    submit_request(&transport, expected_version, sizeof(expected_version));
    const uint8_t version[] = {0x34, 0x12, 0x56, 0x78};
    complete_read(&transport, version, sizeof(version));
    wheel_accessory_service_run(&service, &transport);

    const WheelAccessory *identity = wheel_accessory_service_identity(&service);
    assert(identity->kind == WHEEL_ACCESSORY_EXTENDED);
    assert(identity->initial_status == signed_status(0x8a));
    assert(identity->model == 2);
    assert(identity->version == 0x78561234);
    assert(!service.version_stage);
    assert(service.accessory_type_stage);
    assert(!service.request_pending);
    assert(transport.owner == 0);

    wheel_accessory_service_run(&service, &transport);
    const uint8_t expected_type[] = {2, 0xe1, 7, 1, 0};
    submit_request(&transport, expected_type, sizeof(expected_type));
    const uint8_t accessory_type[] = {0x23};
    complete_read(&transport, accessory_type, sizeof(accessory_type));
    wheel_accessory_service_run(&service, &transport);
    assert(identity->accessory_type == 0x23);
    assert(!service.accessory_type_stage);
}

static void retries_a_failed_stage_without_erasing_identity(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_accessory_apply_probe(&service.accessory, signed_status(0x81), 0x44332211));

    wheel_accessory_service_run(&service, &transport);
    assert(command_transport_request_sent(&transport));
    command_transport_fail(&transport);
    wheel_accessory_service_run(&service, &transport);
    assert(!service.version_stage);
    assert(!service.request_pending);
    assert(service.accessory.kind == WHEEL_ACCESSORY_EXTENDED);
    assert(service.accessory.version == 0x44332211);

    wheel_accessory_service_run(&service, &transport);
    const uint8_t expected_status[] = {2, 0xe1, 0, 1, 0};
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    assert(length == sizeof(expected_status));
    assert(memcmp(request, expected_status, length) == 0);
}

static void waits_for_another_transport_owner(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    command_transport_claim(&transport, 0x22);

    wheel_accessory_service_run(&service, &transport);
    assert(!service.version_stage);
    assert(!service.request_pending);
    assert(transport.owner == 0x22);
    assert(transport.phase == COMMAND_TRANSPORT_IDLE);
}

static void handles_unavailable_services(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);

    wheel_accessory_service_run(0, &transport);
    wheel_accessory_service_run(&service, 0);
    assert(wheel_accessory_service_identity(0) == 0);
}

int main(void) {
    polls_status_then_version_and_applies_identity();
    retries_a_failed_stage_without_erasing_identity();
    waits_for_another_transport_owner();
    handles_unavailable_services();
    return 0;
}
