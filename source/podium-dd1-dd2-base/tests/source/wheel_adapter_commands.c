#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/adapter_commands.h"

static void complete_standard_probe(WheelAdapterCommandService *service, WheelAdapterInput *adapter,
                                    CommandTransport *transport);
static void complete_extended_probe(WheelAdapterCommandService *service, WheelAdapterInput *adapter,
                                    CommandTransport *transport);

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
    uint8_t response[WHEEL_ADAPTER_HOST_CONTROLS_SIZE + 2] = {1, 0};
    assert(length <= sizeof(response) - 2);
    memcpy(response + 2, data, length);
    command_transport_receive(transport, response, length + 2);
}

static void forwards_requested_host_controls(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t status_request[] = {2, 0x2b, 0x00, 2, 0};
    expect_request(&transport, status_request, sizeof(status_request));
    const uint8_t status[] = {0x10, 0};
    complete_read(&transport, status, sizeof(status));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(service.host_controls_pending);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2b, 0xa0, WHEEL_ADAPTER_HOST_CONTROLS_SIZE, 0};
    expect_request(&transport, expected, sizeof(expected));
    uint8_t controls[WHEEL_ADAPTER_HOST_CONTROLS_SIZE];
    for (uint8_t index = 0; index < sizeof(controls); index++) {
        controls[index] = (uint8_t)(index + 1);
    }
    complete_read(&transport, controls, sizeof(controls));
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    uint8_t output[WHEEL_ADAPTER_HOST_CONTROLS_SIZE];
    assert(wheel_adapter_command_service_take_host_controls(&service, output));
    assert(memcmp(output, controls, sizeof(output)) == 0);
    assert(!wheel_adapter_command_service_take_host_controls(&service, output));
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

static void complete_extended_probe(WheelAdapterCommandService *service, WheelAdapterInput *adapter,
                                    CommandTransport *transport) {
    wheel_adapter_command_service_run(service, adapter, transport);
    assert(command_transport_request_sent(transport));
    command_transport_fail(transport);
    wheel_adapter_command_service_run(service, adapter, transport);
    wheel_adapter_command_service_run(service, adapter, transport);
    const uint8_t expected[] = {2, 0x2d, 0x0c, 4, 0};
    expect_request(transport, expected, sizeof(expected));
    const uint8_t probe[] = {1, 0, 0, 0};
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

static void writes_requested_glyphs(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    const uint8_t glyphs[] = {0x11, 0x22, 0x33};
    wheel_adapter_command_service_set_glyphs(&service, glyphs);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(command_transport_request_sent(&transport));
    const uint8_t status[] = {0x20, 0};
    complete_read(&transport, status, sizeof(status));
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    assert(service.glyphs_pending);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2a, 0x06, 0x11, 0x22, 0x33};
    expect_request(&transport, expected, sizeof(expected));
    assert(!service.glyphs_pending);
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
}

static void writes_remote_setup_selections(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_queue_setup_selection(&service, 6);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2a, 0xc0, 6};
    expect_request(&transport, expected, sizeof(expected));
    assert(!service.setup_selection_pending);
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
}

static void writes_remote_tuning_active_state(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_queue_remote_tuning_active(&service, true);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2a, 0x0e, 1};
    expect_request(&transport, expected, sizeof(expected));
    assert(!service.remote_tuning_active_pending);
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
}

static void writes_refresh_state(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_queue_refresh_state(&service, true);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2a, 0x17, 1};
    expect_request(&transport, expected, sizeof(expected));
    assert(!service.refresh_state_pending);
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
}

static void writes_system_display_state(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    wheel_adapter_command_service_queue_display_state(&service, 0x39);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t status_request[] = {2, 0x2b, 0x00, 2, 0};
    expect_request(&transport, status_request, sizeof(status_request));
    const uint8_t status[] = {0, 0};
    complete_read(&transport, status, sizeof(status));
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected[] = {2, 0x2a, 0x18, 0x39};
    expect_request(&transport, expected, sizeof(expected));
    assert(!service.display_state_pending);
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
}

static void writes_extended_output_reports(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_extended_probe(&service, &adapter, &transport);

    uint8_t report_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE];
    uint8_t report_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE];
    for (uint8_t index = 0; index < sizeof(report_four); index++) {
        report_four[index] = (uint8_t)(index + 1);
    }
    for (uint8_t index = 0; index < sizeof(report_five); index++) {
        report_five[index] = (uint8_t)(0x80 + index);
    }
    wheel_adapter_command_service_queue_report_four(&service, report_four);
    wheel_adapter_command_service_queue_report_five(&service, report_five);
    wheel_adapter_command_service_queue_report_six(&service, 0xa5, 0x5a);
    report_four[0] = 0xa5;
    report_four[1] = 0x5a;

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    uint8_t expected_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE + 3] = {2, 0x2c, 0x08};
    memcpy(expected_four + 3, report_four, sizeof(report_four));
    expect_request(&transport, expected_four, sizeof(expected_four));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    uint8_t expected_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE + 3] = {2, 0x2c, 0x09};
    memcpy(expected_five + 3, report_five, sizeof(report_five));
    expect_request(&transport, expected_five, sizeof(expected_five));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected_six[] = {2, 0x2c, 0x19, 0xa5, 0x5a};
    expect_request(&transport, expected_six, sizeof(expected_six));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
}

static void writes_standard_output_reports(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_standard_probe(&service, &adapter, &transport);

    uint8_t report_one[WHEEL_OUTPUT_REPORT_ONE_SIZE];
    uint8_t report_two[WHEEL_OUTPUT_REPORT_TWO_SIZE];
    for (uint8_t index = 0; index < sizeof(report_one); index++) {
        report_one[index] = (uint8_t)(0x40u + index);
    }
    for (uint8_t index = 0; index < sizeof(report_two); index++) {
        report_two[index] = (uint8_t)(0x80u + index);
    }
    wheel_adapter_command_service_queue_report_one(&service, report_one);
    wheel_adapter_command_service_queue_report_two(&service, report_two);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    uint8_t expected_two[WHEEL_OUTPUT_REPORT_TWO_SIZE + 3] = {2, 0x2a, 0x04};
    memcpy(expected_two + 3, report_two, sizeof(report_two));
    expect_request(&transport, expected_two, sizeof(expected_two));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    uint8_t expected_one[WHEEL_OUTPUT_REPORT_ONE_SIZE + 3] = {2, 0x2a, 0x05};
    memcpy(expected_one + 3, report_one, sizeof(report_one));
    expect_request(&transport, expected_one, sizeof(expected_one));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
}

static void writes_extended_text_lines_in_display_order(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_extended_probe(&service, &adapter, &transport);

    const uint8_t second[] = {'M', 'O', 'T', 'O', 'R'};
    const uint8_t first[] = {'B', 'A', 'S', 'E'};
    assert(
        wheel_adapter_command_service_queue_text_line(&service, 2, 0x10, second, sizeof(second)));
    assert(wheel_adapter_command_service_queue_text_line(&service, 1, 0x20, first, sizeof(first)));
    assert(!wheel_adapter_command_service_queue_text_line(&service, 0, 0x10, first, sizeof(first)));

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected_first[] = {2, 0x2c, 0x1a, 1, 0x20, 4, 'B', 'A', 'S', 'E'};
    expect_request(&transport, expected_first, sizeof(expected_first));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected_second[] = {2, 0x2c, 0x1a, 2, 0x10, 5, 'M', 'O', 'T', 'O', 'R'};
    expect_request(&transport, expected_second, sizeof(expected_second));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    wheel_adapter_command_service_queue_text_close(&service);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t expected_close[] = {2, 0x2c, 0x1a, 0, 0x10, 1, ' '};
    expect_request(&transport, expected_close, sizeof(expected_close));
}

static void paces_separate_extended_output_reports(void) {
    WheelAdapterCommandService service;
    WheelAdapterInput adapter;
    CommandTransport transport;
    command_transport_init(&transport);
    wheel_adapter_command_service_init(&service, &adapter);
    complete_extended_probe(&service, &adapter, &transport);

    uint8_t report_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE] = {1};
    uint8_t report_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE] = {2};
    wheel_adapter_command_service_queue_report_four(&service, report_four);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    uint8_t expected_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE + 3] = {2, 0x2c, 0x08, 1};
    expect_request(&transport, expected_four, sizeof(expected_four));
    complete_write(&transport);
    wheel_adapter_command_service_run(&service, &adapter, &transport);

    wheel_adapter_command_service_queue_report_five(&service, report_five);
    const uint8_t status_request[] = {2, 0x2d, 0x00, 2, 0};
    const uint8_t status[] = {0, 0};
    for (uint8_t index = 0; index < 4; index++) {
        wheel_adapter_command_service_run(&service, &adapter, &transport);
        expect_request(&transport, status_request, sizeof(status_request));
        complete_read(&transport, status, sizeof(status));
        wheel_adapter_command_service_run(&service, &adapter, &transport);
    }

    wheel_adapter_command_service_run(&service, &adapter, &transport);
    uint8_t expected[WHEEL_OUTPUT_REPORT_FIVE_SIZE + 3] = {2, 0x2c, 0x09};
    memcpy(expected + 3, report_five, sizeof(report_five));
    expect_request(&transport, expected, sizeof(expected));
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
    assert(adapter.firmware_version[0] == 5);
    assert(adapter.firmware_version[1] == 0);
    assert(adapter.firmware_version[2] == 0);
    assert(adapter.information[0] == 0xc5);
    assert(adapter.information[1] == 0);
    assert(adapter.information[2] == 0);
    assert(adapter.information[3] == 0);

    wheel_adapter_command_service_queue_display(&service, 0x1234);
    wheel_adapter_command_service_queue_display_state(&service, 0x39);
    wheel_adapter_command_service_run(&service, &adapter, &transport);
    const uint8_t status_request[] = {2, 0x2d, 0x00, 2, 0};
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    assert(length == sizeof(status_request));
    assert(memcmp(request, status_request, length) == 0);
    assert(service.display_pending);
    assert(service.display_state_pending);
}

static void rejects_invalid_adapter_command_inputs(void) {
    WheelAdapterCommandService service = {0};
    WheelAdapterInput adapter = {0};
    CommandTransport transport;
    uint8_t text[WHEEL_ADAPTER_TEXT_LENGTH_MAXIMUM + 1] = {0};
    uint8_t report_one[WHEEL_OUTPUT_REPORT_ONE_SIZE] = {0};
    uint8_t report_two[WHEEL_OUTPUT_REPORT_TWO_SIZE] = {0};
    uint8_t report_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE] = {0};
    uint8_t report_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE] = {0};
    uint8_t host_controls[WHEEL_ADAPTER_HOST_CONTROLS_SIZE];

    command_transport_init(&transport);
    wheel_adapter_command_service_queue_display(NULL, 1);
    wheel_adapter_command_service_queue_display(&service, 0);
    wheel_adapter_command_service_set_glyphs(NULL, text);
    wheel_adapter_command_service_set_glyphs(&service, NULL);
    wheel_adapter_command_service_queue_remote_tuning_active(NULL, true);
    wheel_adapter_command_service_queue_refresh_state(NULL, true);
    wheel_adapter_command_service_queue_setup_selection(NULL, 1);
    wheel_adapter_command_service_queue_setup_selection(&service, 0);
    wheel_adapter_command_service_queue_display_state(NULL, 1);
    wheel_adapter_command_service_queue_display_state(&service, 0);
    assert(!wheel_adapter_command_service_queue_text_line(NULL, 1, 0, text, 1));
    assert(!wheel_adapter_command_service_queue_text_line(&service, 1, 0, NULL, 1));
    assert(!wheel_adapter_command_service_queue_text_line(&service, 0, 0, text, 1));
    assert(!wheel_adapter_command_service_queue_text_line(&service, 5, 0, text, 1));
    assert(!wheel_adapter_command_service_queue_text_line(&service, 1, 0, text, sizeof(text)));
    for (uint8_t line = 1; line <= WHEEL_ADAPTER_TEXT_LINE_COUNT; line++) {
        assert(wheel_adapter_command_service_queue_text_line(&service, line, line, text,
                                                             WHEEL_ADAPTER_TEXT_LENGTH_MAXIMUM));
    }
    wheel_adapter_command_service_queue_text_close(NULL);
    wheel_adapter_command_service_queue_report_one(NULL, report_one);
    wheel_adapter_command_service_queue_report_one(&service, NULL);
    wheel_adapter_command_service_queue_report_two(NULL, report_two);
    wheel_adapter_command_service_queue_report_two(&service, NULL);
    wheel_adapter_command_service_queue_report_four(NULL, report_four);
    wheel_adapter_command_service_queue_report_four(&service, NULL);
    wheel_adapter_command_service_queue_report_five(NULL, report_five);
    wheel_adapter_command_service_queue_report_five(&service, NULL);
    wheel_adapter_command_service_queue_report_six(NULL, 1, 2);
    assert(!wheel_adapter_command_service_take_host_controls(NULL, host_controls));
    assert(!wheel_adapter_command_service_take_host_controls(&service, NULL));
    assert(!wheel_adapter_command_service_take_host_controls(&service, host_controls));
    wheel_adapter_command_service_run(NULL, &adapter, &transport);
    wheel_adapter_command_service_run(&service, NULL, &transport);
    wheel_adapter_command_service_run(&service, &adapter, NULL);
}

int main(void) {
    initializes_and_discovers_the_standard_endpoint();
    polls_changed_adapter_inputs();
    applies_motion_direction_priority();
    writes_standard_endpoint_display_reports();
    writes_requested_glyphs();
    writes_remote_setup_selections();
    writes_remote_tuning_active_state();
    writes_refresh_state();
    writes_system_display_state();
    writes_standard_output_reports();
    writes_extended_output_reports();
    writes_extended_text_lines_in_display_order();
    paces_separate_extended_output_reports();
    forwards_requested_host_controls();
    switches_endpoints_after_a_failed_transfer();
    rejects_invalid_adapter_command_inputs();
    return 0;
}
