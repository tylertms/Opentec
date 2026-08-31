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
    expected.vibration_strength = 10;
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

static void test_ignores_invalid_values(void) {
    TuningProfile expected;
    TuningProfile actual;
    uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT];
    tuning_profile_defaults(&expected);
    actual = expected;
    memset(input, UINT8_MAX, sizeof(input));
    input[0] = 127;

    assert(usb_tuning_profile_report_decode(input, &actual));

    assert(memcmp(&actual, &expected, sizeof(expected)) == 0);
}

static void test_encodes_profile_response(void) {
    TuningProfileBank bank;
    uint8_t output[USB_DEVICE_REPORT_SIZE];
    tuning_profile_bank_defaults(&bank);
    bank.active_slot = 3;
    bank.slots[3].force_feedback_strength = 80;

    usb_tuning_profile_report_encode_response(&bank, output);

    assert(output[0] == UINT8_MAX);
    assert(output[1] == 3);
    assert(output[2] == 0x84);
    assert(output[3] == 126);
    assert(output[4] == 80);
    for (uint8_t index = 28; index < USB_DEVICE_REPORT_SIZE; index++) {
        assert(output[index] == 0);
    }

    bank.standard_mode_enabled = false;
    usb_tuning_profile_report_encode_response(&bank, output);
    assert(output[2] == 4);
}

int main(void) {
    test_encodes_factory_profile();
    test_round_trips_manual_rotation();
    test_automatic_rotation_keeps_concrete_range();
    test_rejects_null_inputs();
    test_ignores_invalid_values();
    test_encodes_profile_response();
    return 0;
}
