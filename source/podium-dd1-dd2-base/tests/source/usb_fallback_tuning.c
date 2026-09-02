#include <assert.h>
#include <stdint.h>

#include "profile/tuning.h"
#include "usb/fallback_command.h"
#include "usb/fallback_tuning.h"

static UsbFallbackCommand command(UsbFallbackCommandKind kind, uint8_t value) {
    UsbFallbackCommand output = {.kind = kind, .parameters = {value, 0, 0, 0}};
    output.value = value;
    return output;
}

static void test_applies_transient_setup_one_values(void) {
    TuningProfile retained;
    tuning_profile_defaults(&retained);
    TuningProfile runtime = retained;

    UsbFallbackCommand update = command(USB_FALLBACK_VIBRATION_STRENGTH, 15);
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.vibration_strength == 12);
    assert(retained.vibration_strength == TUNING_VIBRATION_STRENGTH_MAX);

    update = command(USB_FALLBACK_FORCE_FEEDBACK_STRENGTH, 120);
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.force_feedback_strength == 100);

    update = command(USB_FALLBACK_INTERPOLATION, 25);
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.interpolation_filter == 20);
}

static void test_clamps_every_f8_setting_selector(void) {
    static const struct {
        UsbFallbackCommandKind kind;
        uint8_t maximum;
    } cases[] = {
        {USB_FALLBACK_FORCE_FEEDBACK_STRENGTH, 100},
        {USB_FALLBACK_NATURAL_DAMPER, 100},
        {USB_FALLBACK_NATURAL_FRICTION, 100},
        {USB_FALLBACK_NATURAL_INERTIA, 100},
        {USB_FALLBACK_INTERPOLATION, 20},
        {USB_FALLBACK_FORCE_EFFECT_INTENSITY, 100},
        {USB_FALLBACK_FORCE_EFFECT_STRENGTH, 12},
        {USB_FALLBACK_SPRING_EFFECT_STRENGTH, 12},
        {USB_FALLBACK_DAMPER_EFFECT_STRENGTH, 12},
        {USB_FALLBACK_VIBRATION_STRENGTH, 12},
    };
    TuningProfile runtime;
    tuning_profile_defaults(&runtime);

    for (uint8_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        UsbFallbackCommand update = command(cases[index].kind, cases[index].maximum + 1);
        assert(usb_fallback_tuning_apply(&update, 0, &runtime));
        switch (update.kind) {
        case USB_FALLBACK_FORCE_FEEDBACK_STRENGTH:
            assert(runtime.force_feedback_strength == cases[index].maximum);
            break;
        case USB_FALLBACK_NATURAL_DAMPER:
            assert(runtime.natural_damper == cases[index].maximum);
            break;
        case USB_FALLBACK_NATURAL_FRICTION:
            assert(runtime.natural_friction == cases[index].maximum);
            break;
        case USB_FALLBACK_NATURAL_INERTIA:
            assert(runtime.natural_inertia == cases[index].maximum);
            break;
        case USB_FALLBACK_INTERPOLATION:
            assert(runtime.interpolation_filter == cases[index].maximum);
            break;
        case USB_FALLBACK_FORCE_EFFECT_INTENSITY:
            assert(runtime.force_effect_intensity == cases[index].maximum);
            break;
        case USB_FALLBACK_FORCE_EFFECT_STRENGTH:
            assert(runtime.force_effect_strength == cases[index].maximum);
            break;
        case USB_FALLBACK_SPRING_EFFECT_STRENGTH:
            assert(runtime.spring_effect_strength == cases[index].maximum);
            break;
        case USB_FALLBACK_DAMPER_EFFECT_STRENGTH:
            assert(runtime.damper_effect_strength == cases[index].maximum);
            break;
        case USB_FALLBACK_VIBRATION_STRENGTH:
            assert(runtime.vibration_strength == cases[index].maximum);
            break;
        default:
            assert(false);
            break;
        }
    }
}

static void test_decodes_and_applies_wire_update(void) {
    static const uint8_t payload[7] = {0xf8, 0x20, 15, 0, 0, 0, 0};
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = sizeof(payload),
    };
    UsbFallbackCommand update;
    TuningProfile runtime;
    tuning_profile_defaults(&runtime);

    assert(usb_fallback_command_decode(&output, &update));
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.vibration_strength == 12);
}

static void test_rejects_non_setup_one_and_invalid_scale(void) {
    TuningProfile runtime;
    tuning_profile_defaults(&runtime);
    UsbFallbackCommand update = command(USB_FALLBACK_FORCE_FEEDBACK_STRENGTH, 25);

    assert(!usb_fallback_tuning_apply(&update, 1, &runtime));
    assert(runtime.force_feedback_strength == 35);

    update = command(USB_FALLBACK_FORCE_SCALE, 0);
    assert(!usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.force_scale == TUNING_FORCE_SCALE_PEAK);
    update.parameters[0] = 1;
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.force_scale == TUNING_FORCE_SCALE_LINEAR);
    update.parameters[0] = 2;
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.force_scale == TUNING_FORCE_SCALE_PEAK);
}

static void test_decodes_sensitivity_and_range_gate(void) {
    TuningProfile runtime;
    tuning_profile_defaults(&runtime);
    UsbFallbackCommand update = command(USB_FALLBACK_SENSITIVITY, 0);

    update.value = 1300;
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.automatic_rotation == 0);
    assert(runtime.rotation_degrees == 1300);
    assert(usb_fallback_tuning_range_allowed(&runtime));

    update.value = 2530;
    assert(usb_fallback_tuning_apply(&update, 0, &runtime));
    assert(runtime.automatic_rotation == 1);
    assert(runtime.rotation_degrees == TUNING_ROTATION_MAX_DEGREES);
    assert(usb_fallback_tuning_range_allowed(&runtime));

    runtime.automatic_rotation = 0;
    runtime.rotation_degrees = 1080;
    assert(!usb_fallback_tuning_range_allowed(&runtime));
    assert(!usb_fallback_tuning_range_allowed(NULL));
}

static void test_converts_steering_commands_to_physical_travel(void) {
    uint32_t travel;
    UsbFallbackCommand update = command(USB_FALLBACK_STEERING_RANGE_LOW, 0);

    assert(usb_fallback_tuning_steering_travel(&update, &travel));
    assert(travel == 6577);

    update.kind = USB_FALLBACK_STEERING_RANGE_HIGH;
    assert(usb_fallback_tuning_steering_travel(&update, &travel));
    assert(travel == 82880);

    update = command(USB_FALLBACK_STEERING_LIMIT, 89);
    update.value = 89;
    assert(usb_fallback_tuning_steering_travel(&update, &travel));
    assert(travel == 2960);

    update.value = 100;
    assert(usb_fallback_tuning_steering_travel(&update, &travel));
    assert(travel == 3288);

    update.value = 2520;
    assert(usb_fallback_tuning_steering_travel(&update, &travel));
    assert(travel == 82880);

    update.kind = USB_FALLBACK_VIBRATION_STRENGTH;
    assert(!usb_fallback_tuning_steering_travel(&update, &travel));
    assert(!usb_fallback_tuning_steering_travel(NULL, &travel));
    assert(!usb_fallback_tuning_steering_travel(&update, NULL));
}

int main(void) {
    test_applies_transient_setup_one_values();
    test_clamps_every_f8_setting_selector();
    test_decodes_and_applies_wire_update();
    test_rejects_non_setup_one_and_invalid_scale();
    test_decodes_sensitivity_and_range_gate();
    test_converts_steering_commands_to_physical_travel();
    return 0;
}
