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
        assert(command_transport_is_owner(&transport, 0x43));
    }
}

static void test_advances_after_queued_write_without_response(void) {
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
    assert(service.bridge.phase == WHEEL_UPDATER_BRIDGE_READ_DELAY);
    assert(command_transport_is_owner(&transport, 0x43));
    wheel_updater_command_service_run(&service, 0);
    wheel_updater_command_service_run(&service, 0);
    assert(service.bridge.phase == WHEEL_UPDATER_BRIDGE_READ_HEADER);
    assert(command_transport_is_owner(&transport, 0x43));
    const uint8_t *queued = NULL;
    uint16_t queued_length = 0;
    assert(!command_transport_request(&transport, &queued, &queued_length));

    complete_write(&transport, true);
    wheel_updater_command_service_run(&service, 0);
    const uint8_t expected_read[] = {2, 0x23, 0, 1, 0};
    submit(&transport, expected_read, sizeof(expected_read));
}

static void test_stages_read_failure_before_retry(void) {
    CommandTransport transport;
    WheelUpdaterCommandService service;
    const uint8_t request[] = {0x5a, 0xb0};
    const uint8_t expected_write[] = {2, 0x22, 0, 0x5a, 0xb0};
    command_transport_init(&transport);
    wheel_updater_command_service_init(&service, &transport);
    assert(wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_USB, request,
                                               sizeof(request)));

    wheel_updater_command_service_run(&service, 0);
    submit(&transport, expected_write, sizeof(expected_write));
    wheel_updater_command_service_run(&service, 0);
    wheel_updater_command_service_run(&service, 0);
    wheel_updater_command_service_run(&service, 0);
    complete_write(&transport, true);
    wheel_updater_command_service_run(&service, 0);

    const uint8_t expected_read[] = {2, 0x23, 0, 1, 0};
    submit(&transport, expected_read, sizeof(expected_read));
    command_transport_fail(&transport);
    wheel_updater_command_service_run(&service, 0);
    assert(service.failure_pending);
    const uint8_t *queued = NULL;
    uint16_t queued_length = 0;
    assert(!command_transport_request(&transport, &queued, &queued_length));

    wheel_updater_command_service_run(&service, 0);
    assert(!service.failure_pending);
    assert(!command_transport_request(&transport, &queued, &queued_length));
    wheel_updater_command_service_run(&service, 0);
    submit(&transport, expected_read, sizeof(expected_read));
}

static void test_keeps_timed_out_read_pending_until_transport_finishes(void) {
    CommandTransport transport;
    WheelUpdaterCommandService service;
    const uint8_t request[] = {0x5a, 0xb0};
    command_transport_init(&transport);
    wheel_updater_command_service_init(&service, &transport);
    assert(wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_USB, request,
                                               sizeof(request)));

    wheel_updater_command_service_run(&service, 0);
    const uint8_t expected_write[] = {2, 0x22, 0, 0x5a, 0xb0};
    submit(&transport, expected_write, sizeof(expected_write));
    wheel_updater_command_service_run(&service, 0);
    wheel_updater_command_service_run(&service, 0);
    complete_write(&transport, true);
    wheel_updater_command_service_run(&service, 0);

    const uint8_t expected_read[] = {2, 0x23, 0, 1, 0};
    submit(&transport, expected_read, sizeof(expected_read));
    const uint8_t marker[] = {0x5a};
    complete_read(&transport, marker, sizeof(marker));
    wheel_updater_command_service_run(&service, 0);
    submit(&transport, expected_read, sizeof(expected_read));
    const uint8_t retry[] = {0xa1};
    complete_read(&transport, retry, sizeof(retry));
    wheel_updater_command_service_run(&service, 0);
    submit(&transport, expected_read, sizeof(expected_read));

    for (uint16_t tick = 0; tick <= 0x7d0; tick++) {
        wheel_updater_command_service_run(&service, 0);
    }
    assert(wheel_updater_command_service_active(&service));
    wheel_updater_command_service_run(&service, 0);
    assert(service.operation_pending);
    const uint8_t *response = NULL;
    uint8_t response_length = 0;
    assert(wheel_updater_command_service_take_response(&service, &response, &response_length));
    assert(response_length == 2 && response[0] == 0x5a && response[1] == 0xa1);
    assert(!wheel_updater_command_service_active(&service));
    assert(wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_USB, request,
                                               sizeof(request)));

    wheel_updater_command_service_run(&service, 0);
    const uint8_t *queued = NULL;
    uint16_t queued_length = 0;
    assert(!command_transport_request(&transport, &queued, &queued_length));
    const uint8_t stale_marker = 0;
    complete_read(&transport, &stale_marker, sizeof(stale_marker));
    wheel_updater_command_service_run(&service, 0);
    const uint8_t expected_restart_write[] = {2, 0x22, 0, 0x5a, 0xb0};
    submit(&transport, expected_restart_write, sizeof(expected_restart_write));
    complete_write(&transport, true);
    command_transport_release(&transport, 0x43);
}

static void test_executes_zero_length_remote_read(void) {
    CommandTransport transport;
    WheelUpdaterCommandService service;
    const uint8_t request[] = {0x5a, 0xb0};
    command_transport_init(&transport);
    wheel_updater_command_service_init(&service, &transport);
    assert(wheel_updater_command_service_start(&service, WHEEL_UPDATER_TARGET_USB, request,
                                               sizeof(request)));

    wheel_updater_command_service_run(&service, 0);
    const uint8_t expected_write[] = {2, 0x22, 0, 0x5a, 0xb0};
    submit(&transport, expected_write, sizeof(expected_write));
    complete_write(&transport, true);
    wheel_updater_command_service_run(&service, 0);
    wheel_updater_command_service_run(&service, 0);
    wheel_updater_command_service_run(&service, 0);

    const uint8_t expected_read[] = {2, 0x23, 0, 1, 0};
    submit(&transport, expected_read, sizeof(expected_read));
    const uint8_t marker[] = {0x5a};
    complete_read(&transport, marker, sizeof(marker));
    wheel_updater_command_service_run(&service, 0);
    submit(&transport, expected_read, sizeof(expected_read));
    const uint8_t variable[] = {0xa4};
    complete_read(&transport, variable, sizeof(variable));
    wheel_updater_command_service_run(&service, 0);
    const uint8_t expected_length_read[] = {2, 0x23, 0, 2, 0};
    submit(&transport, expected_length_read, sizeof(expected_length_read));
    const uint8_t length[] = {0, 0};
    complete_read(&transport, length, sizeof(length));
    wheel_updater_command_service_run(&service, 0);
    const uint8_t expected_metadata_read[] = {2, 0x23, 0, 2, 0};
    submit(&transport, expected_metadata_read, sizeof(expected_metadata_read));
    const uint8_t metadata[] = {0x34, 0x12};
    complete_read(&transport, metadata, sizeof(metadata));
    wheel_updater_command_service_run(&service, 0);

    const uint8_t expected_empty_read[] = {2, 0x23, 0, 0, 0};
    submit(&transport, expected_empty_read, sizeof(expected_empty_read));
    const uint8_t empty = 0;
    complete_read(&transport, &empty, 0);
    wheel_updater_command_service_run(&service, 0);
    const uint8_t *response = NULL;
    uint8_t response_length = 0;
    assert(wheel_updater_command_service_take_response(&service, &response, &response_length));
    const uint8_t expected_response[] = {0x5a, 0xa4, 0, 0, 0x34, 0x12};
    assert(response_length == sizeof(expected_response));
    assert(memcmp(response, expected_response, sizeof(expected_response)) == 0);
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
    wheel_updater_command_service_run(&service, 103);
    const uint8_t expected_read[] = {2, 0x25, 0, 1, 0};
    submit(&transport, expected_read, sizeof(expected_read));

    const uint8_t marker[] = {0x5a};
    complete_read(&transport, marker, sizeof(marker));
    wheel_updater_command_service_run(&service, 103);
    submit(&transport, expected_read, sizeof(expected_read));
    const uint8_t opcode[] = {0xa2};
    complete_read(&transport, opcode, sizeof(opcode));
    wheel_updater_command_service_run(&service, 103);

    const uint8_t *response = NULL;
    uint8_t response_length = 0;
    assert(wheel_updater_command_service_take_response(&service, &response, &response_length));
    const uint8_t expected_response[] = {0x5a, 0xa2};
    assert(response_length == sizeof(expected_response));
    assert(memcmp(response, expected_response, sizeof(expected_response)) == 0);
    assert(!wheel_updater_command_service_active(&service));
    assert(command_transport_is_owner(&transport, 0x43));
}

int main(void) {
    test_rejects_invalid_service_target_and_request();
    test_maps_command_targets();
    test_advances_after_queued_write_without_response();
    test_stages_read_failure_before_retry();
    test_keeps_timed_out_read_pending_until_transport_finishes();
    test_executes_zero_length_remote_read();
    test_waits_for_other_command_owner();
    test_exchanges_acknowledgement_response();
    return 0;
}
