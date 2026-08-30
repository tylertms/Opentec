#include "profile/tuning_entry.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "profile/tuning_interaction.h"

static const TuningEntryAdjustmentContext default_context = {
    .multi_position_automatic_available = true,
};

static const TuningEntryAvailabilityContext available_context = {
    .interface_mode = 0,
    .wheel_mode = 0x0e,
    .wheel_accessory_kind = WHEEL_ACCESSORY_EXTENDED,
    .pedal_connection = TUNING_PEDALS_TRANSFER,
    .primary_pedal_calibration_active = true,
    .multi_position_supported = true,
    .wheel_axis_report_enabled = true,
    .vibration_mode_compatible = true,
};

static TuningNavigationEvent navigation(TuningNavigationMode mode, int8_t scale) {
    return (TuningNavigationEvent){.mode = mode, .scale = scale};
}

static void test_reports_catalog_and_runtime_limits(void) {
    TuningProfileBank bank;
    TuningEntryLimits limits;
    TuningEntryAdjustmentContext context = default_context;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;

    limits = tuning_entry_limits(TUNING_ENTRY_SETUP, &bank, &context);
    assert(limits.valid);
    assert(limits.minimum == 1 && limits.maximum == 6 && limits.step == 1);
    limits = tuning_entry_limits(TUNING_ENTRY_SENSITIVITY, &bank, &context);
    assert(limits.minimum == -118 && limits.maximum == 126 && limits.step == 1);
    limits = tuning_entry_limits(TUNING_ENTRY_ALTERNATE_BRAKE_FORCE, &bank, &context);
    assert(limits.step == 10);
    context.alternate_brake_fine_step = true;
    limits = tuning_entry_limits(TUNING_ENTRY_ALTERNATE_BRAKE_FORCE, &bank, &context);
    assert(limits.step == 5);
    context.multi_position_automatic_available = false;
    limits = tuning_entry_limits(TUNING_ENTRY_MULTI_POSITION_MODE, &bank, &context);
    assert(limits.minimum == 1);

    bank.standard_mode_enabled = true;
    limits = tuning_entry_limits(TUNING_ENTRY_SETUP, &bank, &context);
    assert(limits.maximum == 2);
    limits = tuning_entry_limits(TUNING_ENTRY_SENSITIVITY, &bank, &context);
    assert(limits.minimum == -109 && limits.maximum == -19);
    limits = tuning_entry_limits(TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH, &bank, &context);
    assert(limits.minimum == 5);
    limits = tuning_entry_limits(TUNING_ENTRY_NATURAL_DAMPER, &bank, &context);
    assert(limits.minimum == 25);

    assert(!tuning_entry_limits(TUNING_ENTRY_COUNT, &bank, &context).valid);
    assert(!tuning_entry_limits(TUNING_ENTRY_SETUP, NULL, &context).valid);
    assert(!tuning_entry_limits(TUNING_ENTRY_SETUP, &bank, NULL).valid);
}

static void test_adjusts_and_clamps_scalar_entries(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;

    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
                               navigation(TUNING_NAVIGATION_ANALOG, 70), &default_context));
    assert(bank.slots[0].force_feedback_strength == 100);
    assert(!tuning_entry_adjust(&bank, TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
                                navigation(TUNING_NAVIGATION_INCREASE, 0), &default_context));
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_FORCE_EFFECT_INTENSITY,
                               navigation(TUNING_NAVIGATION_DECREASE, 0), &default_context));
    assert(bank.slots[0].force_effect_intensity == 90);
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_FORCE_SCALE,
                               navigation(TUNING_NAVIGATION_DECREASE, 0), &default_context));
    assert(bank.slots[0].force_scale == TUNING_FORCE_SCALE_LINEAR);
    assert(!tuning_entry_adjust(&bank, TUNING_ENTRY_FORCE_SCALE,
                                navigation(TUNING_NAVIGATION_PREVIOUS, 0), &default_context));
}

static void test_adjusts_every_scalar_entry(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;

    for (TuningEntry entry = TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH; entry < TUNING_ENTRY_COUNT;
         entry++) {
        if (entry == TUNING_ENTRY_SENSITIVITY) {
            continue;
        }
        bool changed = tuning_entry_adjust(&bank, entry, navigation(TUNING_NAVIGATION_DECREASE, 0),
                                           &default_context);
        if (!changed) {
            changed = tuning_entry_adjust(&bank, entry, navigation(TUNING_NAVIGATION_INCREASE, 0),
                                          &default_context);
        }
        assert(changed);
    }
}

static void test_adjusts_sensitivity_through_automatic_range(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;
    bank.slots[0].automatic_rotation = 0;
    bank.slots[0].rotation_degrees = 2520;

    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SENSITIVITY,
                               navigation(TUNING_NAVIGATION_INCREASE, 0), &default_context));
    assert(bank.slots[0].automatic_rotation == 1);
    assert(bank.slots[0].rotation_degrees == 2520);
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SENSITIVITY,
                               navigation(TUNING_NAVIGATION_DECREASE, 0), &default_context));
    assert(bank.slots[0].automatic_rotation == 0);
    assert(bank.slots[0].rotation_degrees == 2520);
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SENSITIVITY,
                               navigation(TUNING_NAVIGATION_ANALOG, -120), &default_context));
    assert(bank.slots[0].rotation_degrees == 1320);
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SENSITIVITY,
                               navigation(TUNING_NAVIGATION_ANALOG, -120), &default_context));
    assert(bank.slots[0].rotation_degrees == 120);
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SENSITIVITY,
                               navigation(TUNING_NAVIGATION_ANALOG, -120), &default_context));
    assert(bank.slots[0].rotation_degrees == 90);

    bank.standard_mode_enabled = true;
    bank.slots[0].automatic_rotation = 1;
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SENSITIVITY,
                               navigation(TUNING_NAVIGATION_DECREASE, 0), &default_context));
    assert(bank.slots[0].automatic_rotation == 0);
    assert(bank.slots[0].rotation_degrees == 1080);
}

static void test_adjusts_and_activates_setup_selection(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;

    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SETUP, navigation(TUNING_NAVIGATION_ANALOG, 4),
                               &default_context));
    assert(bank.selected_slot == 4);
    assert(bank.active_slot == 4);
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SETUP, navigation(TUNING_NAVIGATION_INCREASE, 0),
                               &default_context));
    assert(bank.selected_slot == 5);
    assert(!tuning_entry_adjust(&bank, TUNING_ENTRY_SETUP,
                                navigation(TUNING_NAVIGATION_INCREASE, 0), &default_context));

    bank.standard_mode_enabled = true;
    bank.selected_slot = 0;
    bank.active_slot = 0;
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_SETUP, navigation(TUNING_NAVIGATION_ANALOG, 5),
                               &default_context));
    assert(bank.selected_slot == 1);
    assert(bank.active_slot == 1);
}

static void test_enforces_security_and_automatic_setup_restrictions(void) {
    TuningProfileBank bank;
    TuningEntryAdjustmentContext context = default_context;
    tuning_profile_bank_defaults(&bank);

    context.security_code_active = true;
    assert(!tuning_entry_adjust(&bank, TUNING_ENTRY_VIBRATION_STRENGTH,
                                navigation(TUNING_NAVIGATION_DECREASE, 0), &context));
    context.security_code_active = false;
    context.automatic_setup_selected = true;
    assert(!tuning_entry_adjust(&bank, TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
                                navigation(TUNING_NAVIGATION_INCREASE, 0), &context));
    assert(tuning_entry_adjust(&bank, TUNING_ENTRY_VIBRATION_STRENGTH,
                               navigation(TUNING_NAVIGATION_DECREASE, 0), &context));
    assert(bank.slots[0].vibration_strength == 0);
    assert(tuning_entry_adjustable_in_automatic_setup(TUNING_ENTRY_BRAKE_PEDAL_CURVE));
    assert(!tuning_entry_adjustable_in_automatic_setup(TUNING_ENTRY_NATURAL_FRICTION));
}

static void test_applies_interface_and_hardware_availability(void) {
    TuningProfileBank bank;
    TuningEntryAvailabilityContext context = available_context;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;

    for (TuningEntry entry = 0; entry < TUNING_ENTRY_COUNT; entry++) {
        bool unavailable =
            entry == TUNING_ENTRY_STEERING_DEADZONE || entry == TUNING_ENTRY_DRIFT_COMPENSATION ||
            entry == TUNING_ENTRY_FULL_FORCE || entry == TUNING_ENTRY_BRAKE_INDICATOR_LEVEL;
        assert(tuning_entry_available(entry, &bank, &context) != unavailable);
    }

    context.interface_mode = 6;
    assert(!tuning_entry_available(TUNING_ENTRY_FORCE_EFFECT_STRENGTH, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_SPRING_EFFECT_STRENGTH, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_DAMPER_EFFECT_STRENGTH, &bank, &context));
    assert(tuning_entry_available(TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH, &bank, &context));
    context.interface_mode = 9;
    assert(!tuning_entry_available(TUNING_ENTRY_SETUP, &bank, &context));

    context = available_context;
    context.legacy_pedal_mode = true;
    assert(!tuning_entry_available(TUNING_ENTRY_FORCE_SCALE, &bank, &context));
    assert(tuning_entry_available(TUNING_ENTRY_BRAKE_INDICATOR_LEVEL, &bank, &context));
    context.wheel_accessory_kind = WHEEL_ACCESSORY_DISCONNECTED;
    assert(!tuning_entry_available(TUNING_ENTRY_NATURAL_DAMPER, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_NATURAL_FRICTION, &bank, &context));
    context = available_context;
    context.pedal_connection = TUNING_PEDALS_UNAVAILABLE;
    assert(!tuning_entry_available(TUNING_ENTRY_BRAKE_FORCE, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_BRAKE_PEDAL_CURVE, &bank, &context));
    context = available_context;
    context.primary_pedal_calibration_active = false;
    assert(!tuning_entry_available(TUNING_ENTRY_ALTERNATE_BRAKE_FORCE, &bank, &context));
    context = available_context;
    context.wheel_mode = 0x10;
    assert(!tuning_entry_available(TUNING_ENTRY_BUTTON_ILLUMINATION, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_DISPLAY_ROTATION, &bank, &context));
}

static void test_filters_entries_for_standard_setups(void) {
    TuningProfileBank bank;
    TuningEntryAvailabilityContext context = available_context;
    tuning_profile_bank_defaults(&bank);

    assert(tuning_entry_available(TUNING_ENTRY_SETUP, &bank, &context));
    assert(tuning_entry_available(TUNING_ENTRY_SENSITIVITY, &bank, &context));
    assert(tuning_entry_available(TUNING_ENTRY_NATURAL_DAMPER, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_NATURAL_FRICTION, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_FORCE_EFFECT_INTENSITY, &bank, &context));
    assert(!tuning_entry_available(TUNING_ENTRY_BRAKE_PEDAL_CURVE, &bank, &context));

    bank.active_slot = 2;
    assert(tuning_entry_available(TUNING_ENTRY_NATURAL_FRICTION, &bank, &context));
    bank.standard_mode_enabled = false;
    assert(tuning_entry_available(TUNING_ENTRY_BRAKE_PEDAL_CURVE, &bank, &context));
}

static void test_navigates_available_entries_in_display_order(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;

    assert(tuning_entry_navigate(TUNING_ENTRY_COUNT, TUNING_NAVIGATION_NEXT, &bank,
                                 &available_context) == TUNING_ENTRY_SETUP);
    assert(tuning_entry_navigate(TUNING_ENTRY_SETUP, TUNING_NAVIGATION_NEXT, &bank,
                                 &available_context) == TUNING_ENTRY_SENSITIVITY);
    assert(tuning_entry_navigate(TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH, TUNING_NAVIGATION_NEXT,
                                 &bank, &available_context) == TUNING_ENTRY_FORCE_SCALE);
    assert(tuning_entry_navigate(TUNING_ENTRY_NATURAL_DAMPER, TUNING_NAVIGATION_PREVIOUS, &bank,
                                 &available_context) == TUNING_ENTRY_FORCE_SCALE);
    assert(tuning_entry_navigate(TUNING_ENTRY_SETUP, TUNING_NAVIGATION_PREVIOUS, &bank,
                                 &available_context) == TUNING_ENTRY_DISPLAY_ROTATION);
    assert(tuning_entry_navigate(TUNING_ENTRY_SETUP, TUNING_NAVIGATION_INCREASE, &bank,
                                 &available_context) == TUNING_ENTRY_SETUP);
}

int main(void) {
    test_reports_catalog_and_runtime_limits();
    test_adjusts_and_clamps_scalar_entries();
    test_adjusts_every_scalar_entry();
    test_adjusts_sensitivity_through_automatic_range();
    test_adjusts_and_activates_setup_selection();
    test_enforces_security_and_automatic_setup_restrictions();
    test_applies_interface_and_hardware_availability();
    test_filters_entries_for_standard_setups();
    test_navigates_available_entries_in_display_order();
    return 0;
}
