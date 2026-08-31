#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "usb/remote_tuning_service.h"
#include "wheel/protocol.h"

static UsbVendorCommand command_for(uint8_t *arguments, uint8_t length) {
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_REMOTE_TUNING,
        .opcode = 5,
        .arguments = arguments,
        .length = length,
    };
}

static void retains_records_and_extends_the_session(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {1, 0x12, 0x34, 0x78, 0x56, 0};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.session_deadline_ms == 60100);
    assert(service.records.records[0].type == 0x12);
    assert(service.records.records[0].selector == 0x34);
    assert(service.records.records[0].value == 0x5678);
}

static void applies_active_state_and_routes_responses(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {2, 1};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, WHEEL_MODE_REMOTE_TUNING_LEGACY,
                                           true, false));
    assert(service.active);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_ACTIVE);
    assert(service.pending_response.link == REMOTE_TUNING_LINK_LEGACY);
    assert(service.pending_response.value == 1);
    assert(!service.active_sync_pending);

    arguments[1] = 0;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, WHEEL_MODE_REMOTE_TUNING_LEGACY,
                                           true, false));
    assert(!service.active);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_INACTIVE);
    assert(service.pending_response.value == 0);
    assert(service.active_sync_pending);

    bool active = true;
    assert(!usb_remote_tuning_service_take_adapter_active(&service, false, &active));
    assert(service.active_sync_pending);
    assert(usb_remote_tuning_service_take_adapter_active(&service, true, &active));
    assert(!active);
    assert(!service.active_sync_pending);
    assert(!usb_remote_tuning_service_take_adapter_active(&service, true, &active));

    usb_remote_tuning_service_init(&service);
    arguments[1] = 1;
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, true));
    assert(service.pending_response.link == REMOTE_TUNING_LINK_EXTENDED);
    assert(service.active_sync_pending);
}

static void applies_menu_and_multi_position_selections(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {4, 1, 6};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.command_type == 1);
    assert(service.menu_selection == 6);

    arguments[2] = 7;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.menu_selection == 6);

    arguments[1] = 2;
    arguments[2] = 11;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.command_type == 2);
    assert(service.multi_position_selection == 11);
}

static void applies_setup_selections(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {4, 3, 6};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.command_type == 0);
    assert(service.setup_selection == 6);
    assert(service.setup_sync_pending);

    arguments[2] = 0;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.setup_selection == 6);
    assert(!service.setup_sync_pending);

    arguments[2] = 6;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    service.menu_selection = 2;
    service.multi_position_selection = 3;
    uint8_t selection = 0;
    assert(usb_remote_tuning_service_take_adapter_setup_selection(&service, &selection));
    assert(selection == 6);
    assert(service.setup_selection == 0);
    assert(service.menu_selection == 0);
    assert(service.multi_position_selection == 0);
    assert(!service.setup_sync_pending);
    assert(!usb_remote_tuning_service_take_adapter_setup_selection(&service, &selection));

    usb_remote_tuning_service_init(&service);
    arguments[2] = 4;
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));
    assert(service.command_type == 0);
    assert(service.setup_index == 4);
    assert(service.setup_page == 4);
    assert(service.encoder_counter == 4);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_SETUP);
    assert(service.pending_response.link == REMOTE_TUNING_LINK_EXTENDED);
    assert(service.pending_response.value == 4);

    arguments[2] = 6;
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));
    assert(service.setup_index == 6);
    assert(service.setup_page == 4);
    assert(service.pending_response.value == 4);

    usb_remote_tuning_service_init(&service);
    arguments[2] = 5;
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, false, false));
    assert(service.command_type == 3);
    assert(service.setup_index == 0);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_NONE);
}

static void applies_encoder_selection_and_clears_other_modes(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {4, 4, 12};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, WHEEL_MODE_REMOTE_TUNING_LEGACY,
                                           true, false));
    assert(service.encoder_selection == 12);
    assert(service.encoder_counter == 12);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_SETUP);
    assert(service.pending_response.link == REMOTE_TUNING_LINK_LEGACY);
    assert(service.pending_response.value == 12);

    service.setup_selection = 1;
    service.menu_selection = 2;
    service.multi_position_selection = 3;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.setup_selection == 0);
    assert(service.menu_selection == 0);
    assert(service.multi_position_selection == 0);
}

static void applies_refresh_requests(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {5, 0};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(!service.refresh_requested);
    assert(service.refresh_sync_pending);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_NONE);

    bool refresh_state = true;
    assert(usb_remote_tuning_service_take_adapter_refresh_state(&service, &refresh_state));
    assert(!refresh_state);
    assert(!service.refresh_sync_pending);

    arguments[1] = 1;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.refresh_requested);
    assert(usb_remote_tuning_service_take_adapter_refresh_state(&service, &refresh_state));
    assert(refresh_state);
    assert(!service.refresh_requested);
    assert(!usb_remote_tuning_service_take_adapter_refresh_state(&service, &refresh_state));

    usb_remote_tuning_service_init(&service);
    arguments[1] = 0;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, WHEEL_MODE_REMOTE_TUNING_LEGACY,
                                           true, false));
    assert(service.refresh_requested);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_REFRESH);
    assert(service.pending_response.link == REMOTE_TUNING_LINK_LEGACY);
    assert(service.pending_response.value == 1);
}

static void takes_pending_responses(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t record_arguments[] = {1, 3, 1, 0x34, 0x12, 0};
    UsbVendorCommand record_command = command_for(record_arguments, sizeof(record_arguments));
    assert(usb_remote_tuning_service_apply(&service, &record_command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));

    uint8_t arguments[] = {2, 1};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));

    RemoteTuningResponse response;
    assert(usb_remote_tuning_service_take_response(&service, WHEEL_MODE_REMOTE_TUNING_EXTENDED,
                                                   &response));
    assert(response.link == REMOTE_TUNING_LINK_EXTENDED);
    assert(response.code == REMOTE_TUNING_RESPONSE_ACTIVE);
    assert(response.value == 1);
    assert(usb_remote_tuning_service_take_response(&service, WHEEL_MODE_REMOTE_TUNING_EXTENDED,
                                                   &response));
    assert(response.code == REMOTE_TUNING_RESPONSE_RECORDS);
    assert(response.record_data_length == 5);
    assert(!usb_remote_tuning_service_take_response(&service, WHEEL_MODE_REMOTE_TUNING_EXTENDED,
                                                    &response));
}

static void routes_generic_records_outside_extended_mode(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {1, 3, 0x81, 0x34, 0x12, 1, 0xaa};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));

    uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE] = {0};
    uint8_t length = 0;
    assert(usb_remote_tuning_service_take_forward_batch(&service, 1, output, &length));
    assert(length == 6);
    assert(output[0] == 3);
    assert(output[1] == 0x81);
    assert(output[2] == 0x34);
    assert(output[3] == 0x12);
    assert(output[4] == 1);
    assert(output[5] == 0xaa);

    usb_remote_tuning_service_init(&service);
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));
    assert(!usb_remote_tuning_service_take_forward_batch(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, output, &length));
    RemoteTuningResponse response;
    assert(usb_remote_tuning_service_take_response(&service, WHEEL_MODE_REMOTE_TUNING_EXTENDED,
                                                   &response));
    assert(response.record_data_length == 6);
}

static void claims_unknown_remote_packets(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {0x77};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.session_deadline_ms == 60100);

    command.kind = USB_VENDOR_COMMAND_TUNING_MENU;
    assert(!usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
}

static void selects_telemetry_and_frames_host_records(void) {
    static const struct {
        UsbRemoteTuningHost host;
        uint8_t marker;
    } cases[] = {
        {USB_REMOTE_TUNING_HOST_NATIVE, 0xff},
        {USB_REMOTE_TUNING_HOST_PLAYSTATION, 0x35},
        {USB_REMOTE_TUNING_HOST_XBOX, 0x36},
    };

    for (uint8_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        UsbRemoteTuningService service;
        usb_remote_tuning_service_init(&service);
        uint8_t active_arguments[] = {2, 1};
        UsbVendorCommand active_command = command_for(active_arguments, sizeof(active_arguments));
        assert(usb_remote_tuning_service_apply(&service, &active_command, 100, 1, true, false));
        uint8_t selection_arguments[] = {4, 2, 1};
        UsbVendorCommand selection_command =
            command_for(selection_arguments, sizeof(selection_arguments));
        assert(usb_remote_tuning_service_apply(&service, &selection_command, 100, 1, true, false));
        assert(service.telemetry.metric == REMOTE_TELEMETRY_SPEED);
        assert(service.command_type == 0);
        assert(service.multi_position_selection == 0);

        uint8_t report[USB_REMOTE_TUNING_HOST_REPORT_SIZE];
        assert(usb_remote_tuning_service_take_host_report(&service, 1, cases[index].host, report));
        const uint8_t expected[] = {2, 0x80, 1, 0, 0x34};
        assert(report[0] == cases[index].marker);
        assert(report[1] == 5);
        assert(report[2] == 1);
        assert(memcmp(report + 3, expected, sizeof(expected)) == 0);
        assert(!usb_remote_tuning_service_take_host_report(&service, 1, cases[index].host, report));
    }
}

static void clears_selected_telemetry(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t active_arguments[] = {2, 1};
    UsbVendorCommand active_command = command_for(active_arguments, sizeof(active_arguments));
    assert(usb_remote_tuning_service_apply(&service, &active_command, 100, 1, true, false));
    uint8_t selection_arguments[] = {4, 2, 1};
    UsbVendorCommand selection_command =
        command_for(selection_arguments, sizeof(selection_arguments));
    assert(usb_remote_tuning_service_apply(&service, &selection_command, 100, 1, true, false));
    uint8_t report[USB_REMOTE_TUNING_HOST_REPORT_SIZE];
    assert(usb_remote_tuning_service_take_host_report(&service, 1, USB_REMOTE_TUNING_HOST_NATIVE,
                                                      report));

    selection_arguments[2] = 11;
    assert(usb_remote_tuning_service_apply(&service, &selection_command, 100, 1, true, false));
    assert(service.telemetry.metric == REMOTE_TELEMETRY_NONE);
    assert(usb_remote_tuning_service_take_host_report(&service, 1, USB_REMOTE_TUNING_HOST_NATIVE,
                                                      report));
    const uint8_t clear[] = {2, 0x80, 0xff, 0xff, 0x34};
    assert(memcmp(report + 3, clear, sizeof(clear)) == 0);
}

static void converts_local_records_to_wheel_telemetry(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t active_arguments[] = {2, 1};
    UsbVendorCommand active_command = command_for(active_arguments, sizeof(active_arguments));
    assert(usb_remote_tuning_service_apply(&service, &active_command, 100, 1, true, false));
    uint8_t selection_arguments[] = {4, 2, 1};
    UsbVendorCommand selection_command =
        command_for(selection_arguments, sizeof(selection_arguments));
    assert(usb_remote_tuning_service_apply(&service, &selection_command, 100, 1, true, false));

    uint8_t host_report[USB_REMOTE_TUNING_HOST_REPORT_SIZE];
    assert(usb_remote_tuning_service_take_host_report(&service, 1, USB_REMOTE_TUNING_HOST_NATIVE,
                                                      host_report));
    uint8_t record_arguments[] = {
        1, 2, 0x00, 1, 0, 2, 123, 0, 2, 0x80, 1, 0, 3, 'm', 'p', 'h',
    };
    UsbVendorCommand record_command = command_for(record_arguments, sizeof(record_arguments));
    assert(usb_remote_tuning_service_apply(&service, &record_command, 100, 1, true, false));

    uint8_t wheel_report[REMOTE_TELEMETRY_REPORT_SIZE];
    assert(usb_remote_tuning_service_take_telemetry_report(&service, 1, wheel_report));
    assert(wheel_report[0] == 1);
    assert(wheel_report[1] == 6);
    assert(memcmp(wheel_report + 2, "123mph", 6) == 0);
    assert(service.records.count == 0);
    assert(!usb_remote_tuning_service_take_telemetry_report(&service, 1, wheel_report));
}

static void processes_valid_local_records_in_extended_mode(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {1, 2, 0, 1, 0, 2, 123, 0};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));

    uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];
    assert(!usb_remote_tuning_service_take_telemetry_report(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, report));
    assert(service.records.count == 0);
}

static void retains_invalid_local_records_in_extended_mode(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    service.refresh_requested = true;
    uint8_t arguments[] = {1, 2, 0, 0, 0, 0};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));

    uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];
    assert(!usb_remote_tuning_service_take_telemetry_report(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, report));
    assert(service.records.count == 1);
    assert(!service.refresh_requested);
}

static void forwards_adapter_host_controls(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t controls[REMOTE_TELEMETRY_REPORT_SIZE] = {
        2, 0x80, 0x34, 0x12, 0x56, 0, 0, 0xaa, 0xbb, 0xcc, 2, 1, 0x78, 0x56, 0x34,
    };
    assert(usb_remote_tuning_service_queue_host_controls(&service, controls) == 2);

    uint8_t report[USB_REMOTE_TUNING_HOST_REPORT_SIZE];
    assert(usb_remote_tuning_service_take_host_report(&service, WHEEL_MODE_REMOTE_TUNING_EXTENDED,
                                                      USB_REMOTE_TUNING_HOST_NATIVE, report));
    assert(report[0] == 0xff && report[1] == 5 && report[2] == 1);
    assert(memcmp(report + 3, controls, REMOTE_TELEMETRY_SUBSCRIPTION_SIZE) == 0);
    assert(memcmp(report + 8, controls + 2 * REMOTE_TELEMETRY_SUBSCRIPTION_SIZE,
                  REMOTE_TELEMETRY_SUBSCRIPTION_SIZE) == 0);
}

static void selects_telemetry_from_physical_controls(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    service.active = true;

    assert(
        usb_remote_tuning_service_update_physical_selection(&service, 1, true, true, false, 1, 0));
    assert(service.telemetry.metric == REMOTE_TELEMETRY_SPEED);
    assert(
        !usb_remote_tuning_service_update_physical_selection(&service, 1, true, true, false, 1, 0));
    assert(
        usb_remote_tuning_service_update_physical_selection(&service, 1, true, true, false, -1, 0));
    assert(service.telemetry.metric == REMOTE_TELEMETRY_NONE);

    usb_remote_tuning_service_init(&service);
    service.active = true;
    assert(!usb_remote_tuning_service_update_physical_selection(&service, 0x10, true, true, false,
                                                                0, 0x04));
    assert(service.physical_input_released);
    assert(usb_remote_tuning_service_update_physical_selection(&service, 0x10, true, true, false, 0,
                                                               0x04));
    assert(service.telemetry.metric == REMOTE_TELEMETRY_SPEED);
    assert(!usb_remote_tuning_service_update_physical_selection(&service, 0x10, true, true, false,
                                                                0, 0x04));
    assert(!usb_remote_tuning_service_update_physical_selection(&service, 0x10, true, true, false,
                                                                0, 0));
    assert(service.physical_input_released);
    assert(usb_remote_tuning_service_update_physical_selection(&service, 0x10, true, true, false, 0,
                                                               0x02));
    assert(service.telemetry.metric == REMOTE_TELEMETRY_NONE);

    service.active = false;
    service.telemetry.metric = REMOTE_TELEMETRY_SPEED;
    assert(
        usb_remote_tuning_service_update_physical_selection(&service, 1, false, true, false, 0, 0));
    assert(service.telemetry.metric == REMOTE_TELEMETRY_NONE);
}

static void advances_legacy_encoder_from_rotary_position(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    service.active = true;
    service.encoder_counter = 1;
    assert(!usb_remote_tuning_service_update_legacy_encoder(&service,
                                                            WHEEL_MODE_REMOTE_TUNING_LEGACY, 12));
    assert(usb_remote_tuning_service_update_legacy_encoder(&service,
                                                           WHEEL_MODE_REMOTE_TUNING_LEGACY, 1));
    assert(service.encoder_selection == 2);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_SETUP);
    assert(service.pending_response.value == 2);
    assert(usb_remote_tuning_service_update_legacy_encoder(&service,
                                                           WHEEL_MODE_REMOTE_TUNING_LEGACY, 12));
    assert(service.encoder_selection == 1);
}

static void navigates_all_six_extended_setup_pages(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    assert(usb_remote_tuning_service_update_setup_navigation(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, 0x10));
    assert(service.setup_page == 1);
    assert(service.pending_response.code == REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE);
    assert(service.pending_response.value == 1);
    assert(!usb_remote_tuning_service_update_setup_navigation(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, 0x10));
    assert(!usb_remote_tuning_service_update_setup_navigation(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, 0x30));
    assert(usb_remote_tuning_service_update_setup_navigation(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, 0x10));
    assert(service.setup_page == 2);
    assert(!usb_remote_tuning_service_update_setup_navigation(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, 0x30));
    assert(usb_remote_tuning_service_update_setup_navigation(
        &service, WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, 0x20));
    assert(service.setup_page == 1);
    assert(!usb_remote_tuning_service_update_setup_navigation(&service, 1, true, 0x10));
}

int main(void) {
    retains_records_and_extends_the_session();
    applies_active_state_and_routes_responses();
    applies_menu_and_multi_position_selections();
    applies_setup_selections();
    applies_encoder_selection_and_clears_other_modes();
    applies_refresh_requests();
    takes_pending_responses();
    routes_generic_records_outside_extended_mode();
    claims_unknown_remote_packets();
    selects_telemetry_and_frames_host_records();
    clears_selected_telemetry();
    converts_local_records_to_wheel_telemetry();
    processes_valid_local_records_in_extended_mode();
    retains_invalid_local_records_in_extended_mode();
    forwards_adapter_host_controls();
    selects_telemetry_from_physical_controls();
    advances_legacy_encoder_from_rotary_position();
    navigates_all_six_extended_setup_pages();
    return 0;
}
