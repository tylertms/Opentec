#include <assert.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/tuning_profile_service.h"
#include "usb/vendor_command.h"

static UsbVendorCommand command(uint8_t arguments[62]) {
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE,
        .opcode = 3,
        .arguments = arguments,
        .length = 62,
    };
}

static void test_applies_and_selects_profile(void) {
    uint8_t arguments[62] = {
        0,  4,  126, 80, 73, 101, 1, 0, 0, 10, 10, 10, 50, 0,
        50, 50, 100, 3,  1,  6,   0, 0, 0, 1,  1,  3,  3,  3,
    };
    UsbVendorCommand update = command(arguments);
    UsbTuningProfileService service;
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    usb_tuning_profile_service_init(&service);
    usb_tuning_profile_service_response_sent(&service);

    UsbTuningProfileAction result =
        usb_tuning_profile_service_apply(&service, &bank, &update, 100);

    assert((result & USB_TUNING_PROFILE_ACTION_CLAIM) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED) != 0);
    assert(bank.selected_slot == 3);
    assert(bank.active_slot == 3);
    assert(bank.slots[3].force_feedback_strength == 80);
    assert(bank.slots[3].vibration_strength == TUNING_VIBRATION_STRENGTH_MAX);
    assert(usb_tuning_profile_service_response_pending(&service));

    arguments[0] = 1;
    arguments[1] = 6;
    result = usb_tuning_profile_service_apply(&service, &bank, &update, 101);
    assert((result & USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED) != 0);
    assert(bank.active_slot == 5);
}

static void test_refresh_and_save_actions(void) {
    uint8_t arguments[62] = {2};
    UsbVendorCommand update = command(arguments);
    UsbTuningProfileService service;
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    usb_tuning_profile_service_init(&service);
    usb_tuning_profile_service_response_sent(&service);

    UsbTuningProfileAction result = usb_tuning_profile_service_apply(&service, &bank, &update, 100);
    assert(result == USB_TUNING_PROFILE_ACTION_CLAIM);
    assert(usb_tuning_profile_service_response_pending(&service));

    arguments[0] = 3;
    result = usb_tuning_profile_service_apply(&service, &bank, &update, 101);
    assert((result & USB_TUNING_PROFILE_ACTION_SAVE) != 0);
}

static void test_requests_response_for_encoded_profile_changes(void) {
    TuningProfile previous;
    TuningProfile current;
    UsbTuningProfileService service;
    tuning_profile_defaults(&previous);
    current = previous;
    usb_tuning_profile_service_init(&service);
    usb_tuning_profile_service_response_sent(&service);

    usb_tuning_profile_service_request_response_if_changed(&service, &previous, &current);
    assert(!usb_tuning_profile_service_response_pending(&service));

    current.natural_damper++;
    usb_tuning_profile_service_request_response_if_changed(&service, &previous, &current);
    assert(usb_tuning_profile_service_response_pending(&service));

    usb_tuning_profile_service_response_sent(&service);
    usb_tuning_profile_service_request_response_if_changed(NULL, &previous, &current);
    usb_tuning_profile_service_request_response_if_changed(&service, NULL, &current);
    usb_tuning_profile_service_request_response_if_changed(&service, &previous, NULL);
    assert(!usb_tuning_profile_service_response_pending(&service));
}

static void test_resets_profiles_with_shared_guard(void) {
    uint8_t arguments[62] = {4};
    UsbVendorCommand update = command(arguments);
    UsbTuningProfileService service;
    TuningProfileBank bank;
    TuningProfile defaults;
    tuning_profile_bank_defaults(&bank);
    tuning_profile_defaults(&defaults);
    bank.slots[0].force_feedback_strength = 90;
    bank.slots[0].force_effect_strength = 7;
    bank.slots[0].vibration_strength = 2;
    bank.slots[0].brake_indicator_level = 22;
    bank.slots[0].brake_force = 12;
    bank.slots[0].alternate_brake_force = 13;
    bank.slots[0].multi_position_mode = TUNING_MULTI_POSITION_PULSE;
    bank.slots[0].paddle_mode = TUNING_CLUTCH_HANDBRAKE;
    for (uint8_t slot = 1; slot < TUNING_PROFILE_SLOT_COUNT; ++slot) {
        bank.slots[slot].force_feedback_strength = (uint8_t)(79 + slot);
    }
    bank.selected_slot = 4;
    bank.active_slot = 4;
    bank.standard_mode_enabled = false;
    bank.automatic_apply_pending = true;
    usb_tuning_profile_service_init(&service);

    UsbTuningProfileAction result = usb_tuning_profile_service_apply(&service, &bank, &update, 100);
    assert((result & USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_CHANGED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_RESET_COMPLETED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) == 0);
    assert(bank.slots[0].force_feedback_strength == 90);
    assert(bank.slots[0].force_effect_strength == 7);
    assert(bank.slots[0].vibration_strength == defaults.vibration_strength);
    assert(bank.slots[0].brake_indicator_level == defaults.brake_indicator_level);
    assert(bank.slots[0].brake_force == defaults.brake_force);
    assert(bank.slots[0].alternate_brake_force == defaults.alternate_brake_force);
    assert(bank.slots[0].multi_position_mode == defaults.multi_position_mode);
    assert(bank.slots[0].paddle_mode == defaults.paddle_mode);
    for (uint8_t slot = 1; slot < TUNING_PROFILE_SLOT_COUNT; ++slot) {
        assert(bank.slots[slot].force_feedback_strength == defaults.force_feedback_strength);
    }
    assert(bank.selected_slot == 0);
    assert(bank.active_slot == 0);
    assert(bank.standard_mode_enabled);
    assert(bank.automatic_apply_pending);
    assert(service.mode_change_after_ms == 0);

    bank.slots[0].force_feedback_strength = 90;
    arguments[0] = 5;
    result = usb_tuning_profile_service_apply(&service, &bank, &update, 1000);
    assert(result == USB_TUNING_PROFILE_ACTION_CLAIM);
    assert((result & USB_TUNING_PROFILE_ACTION_RESET_COMPLETED) == 0);
    assert(bank.slots[0].force_feedback_strength == 90);

    result = usb_tuning_profile_service_apply(&service, &bank, &update, 10100);
    assert((result & USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED) != 0);
    assert(bank.slots[0].force_feedback_strength == 35);
    assert(!bank.automatic_apply_pending);
}

static void test_rate_limits_mode_changes(void) {
    uint8_t arguments[62] = {6};
    UsbVendorCommand update = command(arguments);
    UsbTuningProfileService service;
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    usb_tuning_profile_service_init(&service);

    UsbTuningProfileAction result = usb_tuning_profile_service_apply(&service, &bank, &update, 500);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_CHANGED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) != 0);
    assert(!bank.standard_mode_enabled);
    assert(service.mode_change_after_ms == 2500);

    bank.selected_slot = 4;
    bank.active_slot = 4;
    bank.slots[1].force_feedback_strength = 80;

    result = usb_tuning_profile_service_apply(&service, &bank, &update, 2499);
    assert(result == USB_TUNING_PROFILE_ACTION_CLAIM);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) == 0);
    assert(!bank.standard_mode_enabled);

    result = usb_tuning_profile_service_apply(&service, &bank, &update, 2500);
    assert(result == USB_TUNING_PROFILE_ACTION_CLAIM);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) == 0);
    assert(!bank.standard_mode_enabled);
    assert(service.mode_change_after_ms == 2500);

    result = usb_tuning_profile_service_apply(&service, &bank, &update, 2501);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_CHANGED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED) != 0);
    assert(bank.standard_mode_enabled);
    assert(bank.selected_slot == 0);
    assert(bank.active_slot == 0);
    assert(bank.slots[1].force_feedback_strength == 35);
}

static void test_rejects_other_routes(void) {
    uint8_t arguments[62] = {0};
    UsbVendorCommand update = command(arguments);
    UsbTuningProfileService service;
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    usb_tuning_profile_service_init(&service);
    update.kind = USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT;

    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 100) ==
           USB_TUNING_PROFILE_ACTION_NONE);
}

static void test_rejects_invalid_and_incomplete_profile_commands(void) {
    uint8_t arguments[62] = {0};
    UsbVendorCommand update = command(arguments);
    UsbTuningProfileService service;
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    usb_tuning_profile_service_init(&service);

    assert(usb_tuning_profile_service_apply(NULL, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_NONE);
    assert(usb_tuning_profile_service_apply(&service, NULL, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_NONE);
    assert(usb_tuning_profile_service_apply(&service, &bank, NULL, 0) ==
           USB_TUNING_PROFILE_ACTION_NONE);
    update.arguments = NULL;
    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_NONE);
    update.arguments = arguments;
    update.length = 0;
    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_NONE);

    update.length = 1;
    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_CLAIM);
    update.length = sizeof(arguments);
    arguments[1] = 0;
    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_CLAIM);
    arguments[1] = 7;
    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_CLAIM);
    arguments[0] = 1;
    update.length = 1;
    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_CLAIM);
    arguments[0] = UINT8_MAX;
    update.length = sizeof(arguments);
    assert(usb_tuning_profile_service_apply(&service, &bank, &update, 0) ==
           USB_TUNING_PROFILE_ACTION_CLAIM);
    assert(!usb_tuning_profile_service_response_pending(NULL));
}

int main(void) {
    test_applies_and_selects_profile();
    test_refresh_and_save_actions();
    test_requests_response_for_encoded_profile_changes();
    test_resets_profiles_with_shared_guard();
    test_rate_limits_mode_changes();
    test_rejects_other_routes();
    test_rejects_invalid_and_incomplete_profile_commands();
    return 0;
}
