#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/adapter_commands.h"

static void expect_request(CommandTransport *transport, const uint8_t *expected,
                           uint16_t expected_length) {
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(transport, &request, &length));
    assert(length == expected_length);
    assert(memcmp(request, expected, length) == 0);
    assert(command_transport_request_sent(transport));
}

static void complete_read(CommandTransport *transport, const uint8_t *data, uint16_t length) {
    uint8_t response[6] = {1, 0};
    assert(length <= sizeof(response) - 2);
    memcpy(response + 2, data, length);
    command_transport_receive(transport, response, length + 2);
}

static void complete_write(CommandTransport *transport) {
    const uint8_t response[] = {1};
    command_transport_receive(transport, response, sizeof(response));
}

static void complete_standard_probe(WheelAdapterCommandService *service, WheelAdapterInput *adapter,
                                    CommandTransport *transport) {
    wheel_adapter_command_service_run(service, adapter, transport);
    const uint8_t expected[] = {2, 0x2b, 0x0c, 1, 0};
    expect_request(transport, expected, sizeof(expected));
    const uint8_t probe[] = {1};
    complete_read(transport, probe, sizeof(probe));
    wheel_adapter_command_service_run(service, adapter, transport);
}

static void initializes_and_discovers_the_standard_endpoint(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);

    assert(adapter.axes[0] == 0x7f);
    assert(adapter.axes[1] == 0x80);
    assert(!adapter.connected);
    complete_standard_probe(&service, &adapter, &transport);
    assert(adapter.connected);
    assert(adapter.mode == 0);
    assert(service.phase == WHEEL_ADAPTER_COMMAND_READY);
    assert(transport.owner == 0);
}

static void polls_changed_adapter_inputs(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t status_request[] = {2, 0x2b, 0x00, 2, 0};
    expect_request(&transport, status_request, sizeof(status_request));
    const uint8_t status[] = {0x07, 0x04};
    complete_read(&transport, status, sizeof(status));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(adapter.profile_flags == 0x07);
    assert(adapter.primary_delta == 1);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t button_request[] = {2, 0x2b, 0x01, 3, 0};
    expect_request(&transport, button_request, sizeof(button_request));
    const uint8_t buttons[] = {0x11, 0x22, 0x33};
    complete_read(&transport, buttons, sizeof(buttons));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(memcmp(adapter.buttons, buttons, sizeof(buttons)) == 0);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t axes_request[] = {2, 0x2b, 0x02, 2, 0};
    expect_request(&transport, axes_request, sizeof(axes_request));
    const uint8_t axes[] = {0x44, 0x55};
    complete_read(&transport, axes, sizeof(axes));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(memcmp(adapter.axes, axes, sizeof(axes)) == 0);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t rotary_request[] = {2, 0x2b, 0x03, 1, 0};
    expect_request(&transport, rotary_request, sizeof(rotary_request));
    const uint8_t rotary[] = {0x66};
    complete_read(&transport, rotary, sizeof(rotary));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(adapter.rotary_positions[0] == 0x66);
    assert(adapter.rotary_positions[1] == 0);
    assert(adapter.rotary_positions[2] == 0);
    assert(service.pending_inputs == 0);
}

static void applies_motion_direction_priority(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(command_transport_request_sent(&transport));
    const uint8_t decrement[] = {0, 0x08};
    complete_read(&transport, decrement, sizeof(decrement));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(adapter.primary_delta == -1);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(command_transport_request_sent(&transport));
    const uint8_t both[] = {0, 0x0c};
    complete_read(&transport, both, sizeof(both));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(adapter.primary_delta == 0);
}

static void writes_standard_endpoint_display_reports(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_queue_display(&service, 0x5678);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2a, 0x0f, 0x78, 0x56, 0};
    expect_request(&transport, expected, sizeof(expected));
    assert(!service.display_pending);
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(service.phase == WHEEL_ADAPTER_COMMAND_READY);
    assert(transport.owner == 0);
}

static void switches_endpoints_after_a_failed_transfer(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(command_transport_request_sent(&transport));
    command_transport_fail(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(service.endpoint_index == 1);
    assert(adapter.mode == 1);
    assert(!adapter.connected);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2d, 0x0c, 4, 0};
    expect_request(&transport, expected, sizeof(expected));
    const uint8_t probe[] = {0xc5, 0, 0, 0};
    complete_read(&transport, probe, sizeof(probe));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(adapter.connected);

    wheel_adapter_command_service_queue_display(&service, 0x1234);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t status_request[] = {2, 0x2d, 0x00, 2, 0};
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    assert(length == sizeof(status_request));
    assert(memcmp(request, status_request, length) == 0);
    assert(service.display_pending);
}

int main(void) {
    initializes_and_discovers_the_standard_endpoint();
    polls_changed_adapter_inputs();
    applies_motion_direction_priority();
    writes_standard_endpoint_display_reports();
    switches_endpoints_after_a_failed_transfer();
    return 0;
}
