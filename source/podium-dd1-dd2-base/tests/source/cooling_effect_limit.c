#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "cooling/effect_limit.h"

static CoolingEffectStrengths maximum_strengths(void) {
    return (CoolingEffectStrengths){
        .force = 12,
        .spring = 11,
        .damper = 12,
    };
}

static void test_standard_limit(void) {
    CoolingController controller;
    CoolingEffectLimit limit;
    CoolingEffectStrengths strengths = maximum_strengths();
    cooling_controller_init(&controller, false);
    cooling_effect_limit_init(&limit);

    cooling_effect_limit_update(&limit, &strengths, &controller, 116.0f, false, 0);
    assert(limit.phase == COOLING_EFFECT_LIMIT_STANDARD);
    assert(!limit.active);
    assert(!cooling_effect_limit_resistance_profile_active(&limit));
    assert(strengths.force == 12);
    cooling_effect_limit_update(&limit, &strengths, &controller, 116.0f, false, 0);
    assert(limit.active);
    assert(cooling_effect_limit_resistance_profile_active(&limit));
    assert(strengths.force == 10);
    assert(strengths.spring == 10);
    assert(strengths.damper == 10);

    cooling_effect_limit_update(&limit, &strengths, &controller, 100.0f, false, 0);
    assert(limit.phase == COOLING_EFFECT_LIMIT_STANDARD);
    cooling_effect_limit_update(&limit, &strengths, &controller, 99.0f, false, 0);
    assert(limit.phase == COOLING_EFFECT_LIMIT_INACTIVE);
    assert(limit.active);
    assert(cooling_effect_limit_resistance_profile_active(&limit));
    assert(strengths.force == 12);
    assert(strengths.spring == 11);
    assert(strengths.damper == 12);
    cooling_effect_limit_update(&limit, &strengths, &controller, 99.0f, false, 0);
    assert(!limit.active);
    assert(!cooling_effect_limit_resistance_profile_active(&limit));
}

static void test_managed_limit(void) {
    CoolingController controller;
    CoolingEffectLimit limit;
    CoolingEffectStrengths strengths = maximum_strengths();
    cooling_controller_init(&controller, true);
    cooling_effect_limit_init(&limit);
    controller.phase = COOLING_PHASE_MANAGED_WINDOW;
    controller.primary_deadline_ms = 1000;
    cooling_controller_set_secondary_delay_seconds(&controller, 1);

    cooling_effect_limit_update(&limit, &strengths, &controller, 130.0f, true, 2000);
    assert(limit.phase == COOLING_EFFECT_LIMIT_INACTIVE);
    cooling_effect_limit_update(&limit, &strengths, &controller, 130.0f, true, 2001);
    assert(limit.phase == COOLING_EFFECT_LIMIT_MANAGED);
    cooling_effect_limit_update(&limit, &strengths, &controller, 130.0f, true, 2002);
    assert(limit.active);
    assert(cooling_effect_limit_resistance_profile_active(&limit));
    assert(strengths.force == 10);

    cooling_controller_set_low_threshold_offset(&controller, -5);
    cooling_effect_limit_update(&limit, &strengths, &controller, 110.0f, true, 2003);
    assert(limit.phase == COOLING_EFFECT_LIMIT_MANAGED);
    cooling_effect_limit_update(&limit, &strengths, &controller, 109.0f, true, 2004);
    assert(limit.phase == COOLING_EFFECT_LIMIT_INACTIVE);
    assert(cooling_effect_limit_resistance_profile_active(&limit));
    assert(strengths.force == 12);
    cooling_effect_limit_update(&limit, &strengths, &controller, 109.0f, true, 2005);
    assert(!cooling_effect_limit_resistance_profile_active(&limit));
}

static void test_values_at_or_below_limit(void) {
    CoolingController controller;
    CoolingEffectLimit limit;
    CoolingEffectStrengths strengths = {
        .force = 10,
        .spring = 9,
        .damper = 8,
    };
    cooling_controller_init(&controller, false);
    cooling_effect_limit_init(&limit);
    limit.phase = COOLING_EFFECT_LIMIT_STANDARD;
    cooling_effect_limit_update(&limit, &strengths, &controller, 110.0f, false, 0);
    assert(strengths.force == 10);
    assert(strengths.spring == 9);
    assert(strengths.damper == 8);
}

int main(void) {
    test_standard_limit();
    test_managed_limit();
    test_values_at_or_below_limit();
    return 0;
}
