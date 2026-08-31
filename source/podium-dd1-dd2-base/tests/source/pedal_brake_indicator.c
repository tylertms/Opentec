#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "pedal/brake_indicator.h"

static void test_activates_at_scaled_threshold(void) {
    PedalBrakeIndicator indicator;
    pedal_brake_indicator_init(&indicator);

    assert(pedal_brake_indicator_update(&indicator, 50, UINT16_C(0x7eff), false) ==
           PEDAL_BRAKE_INDICATOR_NO_UPDATE);
    assert(pedal_brake_indicator_update(&indicator, 50, UINT16_C(0x7fff), false) == 0x3f);
    assert(pedal_brake_indicator_update(&indicator, 50, UINT16_C(0xffff), false) == 0x3f);
}

static void test_selects_legacy_transport(void) {
    PedalBrakeIndicator indicator;
    pedal_brake_indicator_init(&indicator);

    assert(pedal_brake_indicator_update(&indicator, 100, UINT16_MAX, true) == UINT8_MAX);
}

static void test_releases_once_below_threshold(void) {
    PedalBrakeIndicator indicator;
    pedal_brake_indicator_init(&indicator);

    assert(pedal_brake_indicator_update(&indicator, 1, UINT16_C(0x02ff), false) == 0x3f);
    assert(pedal_brake_indicator_update(&indicator, 1, UINT16_C(0x01ff), false) == 0);
    assert(pedal_brake_indicator_update(&indicator, 1, UINT16_C(0x01ff), false) ==
           PEDAL_BRAKE_INDICATOR_NO_UPDATE);
}

static void test_disables_level_101(void) {
    PedalBrakeIndicator indicator;
    pedal_brake_indicator_init(&indicator);

    assert(pedal_brake_indicator_update(&indicator, 0, 0, false) == 0x3f);
    assert(pedal_brake_indicator_update(&indicator, PEDAL_BRAKE_INDICATOR_DISABLED, UINT16_MAX,
                                        false) == 0);
}

int main(void) {
    test_activates_at_scaled_threshold();
    test_selects_legacy_transport();
    test_releases_once_below_threshold();
    test_disables_level_101();
    return 0;
}
