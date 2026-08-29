#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/updater_command_service.h"

static void submit(CommandTransport *transport, const uint8_t *expected, uint16_t expected_length) {
    const uint8_t *request = NULL;
    uint16_t length = 0;
    assert(command_transport_request(transport, &request, &length));
    assert(length == expected_length);
    assert(memcmp(request, expected, length) == 0);
    assert(command_transport_request_sent(transport));
}

static void complete_write(CommandTransport *transport, bool accepted) {
    const uint8_t response[] = {accepted ? 1 : 0};
    command_transport_receive(transport, response, sizeof(response));
}

static void complete_read(CommandTransport *transport, const uint8_t *data, uint8_t length) {
    uint8_t response[62] = {1, 0};
    memcpy(response + 2, data, length);
    command_transport_receive(transport, response, (uint16_t)length + 2);
}

static void test_rejects_invalid_service_target_and_request(void) {
    CommandTransport transport;
    WheelUpdaterCommandService service;
    const uint8_t request[] = {0x5a, 0xb0};
    command_transport_init(&transport);
    wheel_updater_command_service_init(&service, &transport);

    assert(!wheel_updater_command_service_start(NULL, WHEEL_UPDATER_TARGET_USB, request,
                                                sizeof(request)));
    wheel_updater_command_service_init(&service, NULL);
    assert(!wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_USB, request,
                                                sizeof(request)));
    wheel_updater_command_service_init(&service, &transport);
    assert(!wheel_updater_command_service_start(&service, (WheelUpdaterTarget)0x10, request,
                                                sizeof(request)));
    assert(!wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_USB, NULL,
                                                sizeof(request)));
    assert(!wheel_updater_command_service_active(NULL));
    wheel_updater_command_service_run(NULL, 0);
}

static void test_maps_command_targets(void) {
    static const WheelUpdaterTarget targets[] = {
        WHEEL_UPDATER_TARGET_USB,
        WHEEL_UPDATER_TARGET_PROTOCOL,
    };
    const uint8_t request[] = {0x5a, 0xa1};
    for (uint8_t index = 0; index < sizeof(targets) / sizeof(targets[0]); index++) {
        CommandTransport transport;
        WheelUpdaterCommandService service;
        command_transport_init(&transport);
        wheel_updater_command_service_init(&service, &transport);
        assert(wheel_updater_command_service_start(&service, targets[index], request,
                                                   sizeof(request)));

        wheel_updater_command_service_run(&service, 0);
        const uint8_t expected[] = {2, (uint8_t)(targets[index] << 1), 0, 0x5a, 0xa1};
        submit(&transport, expected, sizeof(expected));
        complete_write(&transport, true);
        wheel_updater_command_service_run(&service, 0);
        assert(!wheel_updater_command_service_active(&service));
        assert(command_transport_is_owner(&transport, 0));
    }
}

static void test_stops_rejected_write(void) {
    CommandTransport transport;
    WheelUpdaterCommandService service;
    const uint8_t request[] = {0x5a, 0xb0};
    const uint8_t expected[] = {2, 0x22, 0, 0x5a, 0xb0};
    command_transport_init(&transport);
    wheel_updater_command_service_init(&service, &transport);
    assert(wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_USB, request,
                                               sizeof(request)));

    wheel_updater_command_service_run(&service, 0);
    submit(&transport, expected, sizeof(expected));
    complete_write(&transport, false);
    wheel_updater_command_service_run(&service, 0);
    assert(!wheel_updater_command_service_active(&service));
    assert(command_transport_is_owner(&transport, 0));
}

static void test_waits_for_other_command_owner(void) {
    CommandTransport transport;
    WheelUpdaterCommandService service;
    const uint8_t request[] = {0x5a, 0xa1};
    command_transport_init(&transport);
    command_transport_claim(&transport, 0x42);
    wheel_updater_command_service_init(&service, &transport);
    assert(wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_PROTOCOL, request,
                                               sizeof(request)));

    wheel_updater_command_service_run(&service, 0);
    assert(command_transport_is_owner(&transport, 0x42));
    const uint8_t *queued = NULL;
    uint16_t length = 0;
    assert(!command_transport_request(&transport, &queued, &length));
    command_transport_release(&transport, 0x42);
    wheel_updater_command_service_run(&service, 0);
    assert(command_transport_request(&transport, &queued, &length));
}

static void test_exchanges_acknowledgement_response(void) {
    CommandTransport transport;
    WheelUpdaterCommandService service;
    const uint8_t request[] = {0x5a, 0xb0, 0x33};
    command_transport_init(&transport);
    wheel_updater_command_service_init(&service, &transport);
    assert(wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_PROTOCOL, request,
                                               sizeof(request)));

    wheel_updater_command_service_run(&service, 100);
    const uint8_t expected_write[] = {2, 0x24, 0, 0x5a, 0xb0, 0x33};
    submit(&transport, expected_write, sizeof(expected_write));
    complete_write(&transport, true);
    wheel_updater_command_service_run(&service, 100);
    wheel_updater_command_service_run(&service, 101);
    wheel_updater_command_service_run(&service, 102);
    const uint8_t expected_read[] = {2, 0x25, 0, 1, 0};
    submit(&transport, expected_read, sizeof(expected_read));

    const uint8_t marker[] = {0x5a};
    complete_read(&transport, marker, sizeof(marker));
    wheel_updater_command_service_run(&service, 102);
    submit(&transport, expected_read, sizeof(expected_read));
    const uint8_t opcode[] = {0xa2};
    complete_read(&transport, opcode, sizeof(opcode));
    wheel_updater_command_service_run(&service, 102);

    const uint8_t *response = NULL;
    uint8_t response_length = 0;
    assert(wheel_updater_command_service_take_response(&service, &response, &response_length));
    const uint8_t expected_response[] = {0x5a, 0xa2};
    assert(response_length == sizeof(expected_response));
    assert(memcmp(response, expected_response, sizeof(expected_response)) == 0);
    assert(!wheel_updater_command_service_active(&service));
    assert(command_transport_is_owner(&transport, 0));
}

int main(void) {
    test_rejects_invalid_service_target_and_request();
    test_maps_command_targets();
    test_stops_rejected_write();
    test_waits_for_other_command_owner();
    test_exchanges_acknowledgement_response();
    return 0;
}
