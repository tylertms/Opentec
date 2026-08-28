#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

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
    assert(service.records.records[31].type == 0x12);
    assert(service.records.records[31].selector == 0x34);
    assert(service.records.records[31].value == 0x5678);
}

static void applies_active_state_and_routes_responses(void) {
    UsbRemoteTuningService service;
    usb_remote_tuning_service_init(&service);
    uint8_t arguments[] = {2, 1};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_service_apply(&service, &command, 100, WHEEL_MODE_REMOTE_TUNING_LEGACY,
                                           true, false));
    assert(service.active);
    assert(service.pending_response == USB_REMOTE_TUNING_RESPONSE_ACTIVE);
    assert(service.response_target == USB_REMOTE_TUNING_RESPONSE_TARGET_LEGACY);
    assert(!service.active_sync_pending);

    arguments[1] = 0;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, WHEEL_MODE_REMOTE_TUNING_LEGACY,
                                           true, false));
    assert(!service.active);
    assert(service.pending_response == USB_REMOTE_TUNING_RESPONSE_INACTIVE);
    assert(service.active_sync_pending);

    usb_remote_tuning_service_init(&service);
    arguments[1] = 1;
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, true));
    assert(service.response_target == USB_REMOTE_TUNING_RESPONSE_TARGET_EXTENDED);
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
    assert(service.vendor_response_pending);

    service.vendor_response_pending = false;
    arguments[2] = 7;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.menu_selection == 6);
    assert(!service.vendor_response_pending);

    arguments[1] = 2;
    arguments[2] = 11;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.command_type == 2);
    assert(service.multi_position_selection == 11);
    assert(service.vendor_response_pending);
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
    assert(service.vendor_response_pending);

    service.vendor_response_pending = false;
    arguments[2] = 0;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.setup_selection == 6);
    assert(!service.setup_sync_pending);
    assert(!service.vendor_response_pending);

    usb_remote_tuning_service_init(&service);
    arguments[2] = 4;
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, true, false));
    assert(service.command_type == 0);
    assert(service.setup_index == 4);
    assert(service.encoder_counter == 4);
    assert(service.pending_response == USB_REMOTE_TUNING_RESPONSE_SETUP);
    assert(service.response_target == USB_REMOTE_TUNING_RESPONSE_TARGET_EXTENDED);

    usb_remote_tuning_service_init(&service);
    arguments[2] = 5;
    assert(usb_remote_tuning_service_apply(&service, &command, 100,
                                           WHEEL_MODE_REMOTE_TUNING_EXTENDED, false, false));
    assert(service.command_type == 3);
    assert(service.setup_index == 0);
    assert(service.pending_response == USB_REMOTE_TUNING_RESPONSE_NONE);
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
    assert(service.pending_response == USB_REMOTE_TUNING_RESPONSE_SETUP);
    assert(service.response_target == USB_REMOTE_TUNING_RESPONSE_TARGET_LEGACY);

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
    assert(service.pending_response == USB_REMOTE_TUNING_RESPONSE_NONE);

    arguments[1] = 1;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, 1, true, false));
    assert(service.refresh_requested);

    usb_remote_tuning_service_init(&service);
    arguments[1] = 0;
    assert(usb_remote_tuning_service_apply(&service, &command, 100, WHEEL_MODE_REMOTE_TUNING_LEGACY,
                                           true, false));
    assert(service.refresh_requested);
    assert(service.pending_response == USB_REMOTE_TUNING_RESPONSE_REFRESH);
    assert(service.response_target == USB_REMOTE_TUNING_RESPONSE_TARGET_LEGACY);
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

int main(void) {
    retains_records_and_extends_the_session();
    applies_active_state_and_routes_responses();
    applies_menu_and_multi_position_selections();
    applies_setup_selections();
    applies_encoder_selection_and_clears_other_modes();
    applies_refresh_requests();
    claims_unknown_remote_packets();
    return 0;
}
