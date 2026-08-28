#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/vibration.h"

static void assert_channels(const WheelVibrationOutput *output, uint8_t expected) {
    for (uint8_t channel = 0; channel < WHEEL_VIBRATION_CHANNEL_COUNT; channel++) {
        assert(output->channels[channel] == expected);
    }
}

static void test_tracks_brake_position(void) {
    WheelVibrationOutput output;

    wheel_vibration_from_brake(&output, UINT16_C(0x4067), 10, 6, true);

    assert_channels(&output, 0x40);
}

static void test_clamps_standard_modes(void) {
    WheelVibrationOutput output;

    wheel_vibration_from_brake(&output, UINT16_MAX, 1, 6, true);
    assert_channels(&output, 75);
    wheel_vibration_from_brake(&output, UINT16_MAX, 10, 6, true);
    assert_channels(&output, UINT8_MAX);
}

static void test_clamps_low_range_modes(void) {
    WheelVibrationOutput output;

    wheel_vibration_from_brake(&output, UINT16_MAX, 1, 0x0a, true);
    assert_channels(&output, 15);
    wheel_vibration_from_brake(&output, UINT16_MAX, 10, 0x1c, true);
    assert_channels(&output, 105);
}

static void test_disables_inactive_and_nonoutput_strengths(void) {
    WheelVibrationOutput output;

    wheel_vibration_from_brake(&output, UINT16_MAX, 10, 6, false);
    assert_channels(&output, 0);
    wheel_vibration_from_brake(&output, UINT16_MAX, 0, 6, true);
    assert_channels(&output, 0);
    wheel_vibration_from_brake(&output, UINT16_MAX, 11, 6, true);
    assert_channels(&output, 0);
    wheel_vibration_from_brake(&output, UINT16_MAX, 12, 6, true);
    assert_channels(&output, 0);
}

int main(void) {
    test_tracks_brake_position();
    test_clamps_standard_modes();
    test_clamps_low_range_modes();
    test_disables_inactive_and_nonoutput_strengths();
    return 0;
}
