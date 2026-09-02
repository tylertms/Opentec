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

    UsbTuningProfileAction result = usb_tuning_profile_service_apply(&service, &bank, &update, 100);

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

static void test_resets_profiles_with_shared_guard(void) {
    uint8_t arguments[62] = {4};
    UsbVendorCommand update = command(arguments);
    UsbTuningProfileService service;
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.slots[0].force_feedback_strength = 90;
    bank.slots[1].force_feedback_strength = 80;
    bank.standard_mode_enabled = false;
    bank.automatic_apply_pending = true;
    usb_tuning_profile_service_init(&service);

    UsbTuningProfileAction result = usb_tuning_profile_service_apply(&service, &bank, &update, 100);
    assert((result & USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_CHANGED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_RESET_COMPLETED) != 0);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) == 0);
    assert(bank.slots[0].force_feedback_strength == 35);
    assert(bank.slots[1].force_feedback_strength == 35);
    assert(bank.standard_mode_enabled);
    assert(bank.automatic_apply_pending);

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

    bank.selected_slot = 4;
    bank.active_slot = 4;
    bank.slots[1].force_feedback_strength = 80;

    result = usb_tuning_profile_service_apply(&service, &bank, &update, 2499);
    assert(result == USB_TUNING_PROFILE_ACTION_CLAIM);
    assert((result & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) == 0);
    assert(!bank.standard_mode_enabled);

    result = usb_tuning_profile_service_apply(&service, &bank, &update, 2500);
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
    test_resets_profiles_with_shared_guard();
    test_rate_limits_mode_changes();
    test_rejects_other_routes();
    test_rejects_invalid_and_incomplete_profile_commands();
    return 0;
}
