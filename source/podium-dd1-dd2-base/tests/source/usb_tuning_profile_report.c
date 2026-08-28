#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "profile/tuning.h"
#include "usb/tuning_profile_report.h"

static void test_encodes_factory_profile(void) {
    static const uint8_t expected[USB_TUNING_PROFILE_VALUE_COUNT] = {
        126, 35, 10, 101, 1, 0, 0, 10, 10, 10, 50, 0, 50, 50, 100, 3, 1, 6, 0, 0, 1, 1, 3, 3, 3,
    };
    TuningProfile profile;
    uint8_t encoded[USB_TUNING_PROFILE_VALUE_COUNT];
    tuning_profile_defaults(&profile);

    usb_tuning_profile_report_encode(&profile, encoded);

    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void test_round_trips_manual_rotation(void) {
    TuningProfile expected;
    TuningProfile decoded;
    uint8_t encoded[USB_TUNING_PROFILE_VALUE_COUNT];
    tuning_profile_defaults(&expected);
    expected.automatic_rotation = 0;
    expected.rotation_degrees = 1080;
    expected.vibration_strength = 73;
    expected.natural_friction = 42;
    expected.full_force_enabled = 1;
    decoded = expected;
    decoded.rotation_degrees = 90;

    usb_tuning_profile_report_encode(&expected, encoded);
    assert(encoded[0] == UINT8_C(0xed));
    assert(usb_tuning_profile_report_decode(encoded, &decoded));

    assert(memcmp(&decoded, &expected, sizeof(expected)) == 0);
}

static void test_automatic_rotation_keeps_concrete_range(void) {
    TuningProfile profile;
    uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT] = {
        126, 35, 10, 101, 1, 0, 0, 10, 10, 10, 50, 0, 50, 50, 100, 3, 1, 6, 0, 0, 1, 1, 3, 3, 3,
    };
    tuning_profile_defaults(&profile);
    profile.automatic_rotation = 0;
    profile.rotation_degrees = 900;

    assert(usb_tuning_profile_report_decode(input, &profile));
    assert(profile.automatic_rotation == 1);
    assert(profile.rotation_degrees == 900);
}

static void test_rejects_null_inputs(void) {
    TuningProfile profile;
    uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT] = {0};
    tuning_profile_defaults(&profile);

    assert(!usb_tuning_profile_report_decode(NULL, &profile));
    assert(!usb_tuning_profile_report_decode(input, NULL));
}

int main(void) {
    test_encodes_factory_profile();
    test_round_trips_manual_rotation();
    test_automatic_rotation_keeps_concrete_range();
    test_rejects_null_inputs();
    return 0;
}
