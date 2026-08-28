#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/capability.h"

static void test_caches_and_maps_report_capabilities(void) {
    WheelCapabilityState state = {.report_flags = 0xffe1};

    wheel_capability_update(&state, 6, 0x34, 0x3f);

    assert(state.capability_flags == 0x3f34);
    assert(state.report_flags == 0xffff);
    assert(state.calibration_available);
    assert(state.tuning_menu_available);

    wheel_capability_update(&state, 6, 0x12, 0);
    assert(state.capability_flags == 0x0012);
    assert(state.report_flags == 0xffe1);
    assert(!state.calibration_available);
    assert(!state.tuning_menu_available);
}

static void test_applies_calibration_mode_defaults(void) {
    static const uint8_t forced_available[] = {5, 7, 8, 0x10, 0x12};
    static const uint8_t forced_unavailable[] = {9, 0x0b, 0x11, 0x15, 0x16, 0x1d};
    WheelCapabilityState state = {0};

    for (uint8_t index = 0; index < sizeof(forced_available); index++) {
        wheel_capability_update(&state, forced_available[index], 0, 0);
        assert(state.calibration_available);
    }
    for (uint8_t index = 0; index < sizeof(forced_unavailable); index++) {
        wheel_capability_update(&state, forced_unavailable[index], 0, UINT8_MAX);
        assert(!state.calibration_available);
    }

    wheel_capability_update(&state, 6, 0, 1);
    assert(state.calibration_available);
    wheel_capability_update(&state, 6, 0, 0);
    assert(!state.calibration_available);
}

int main(void) {
    test_caches_and_maps_report_capabilities();
    test_applies_calibration_mode_defaults();
    return 0;
}
