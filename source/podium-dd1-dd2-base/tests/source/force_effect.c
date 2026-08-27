#include <assert.h>
#include <stdint.h>

#include "force_feedback/effect.h"

static void test_constant_force(void) {
    assert(force_effect_constant(32000, 10000) == 32000);
    assert(force_effect_constant(-32000, 5000) == -16000);
    assert(force_effect_constant(32000, 0) == 0);
    assert(force_effect_constant(32000, UINT16_MAX) == 32000);
}

static void test_spring_deadband_and_direction(void) {
    const ForceSpringEffect effect = {
        .center = 100,
        .deadband = 50,
        .saturation_distance = 1000,
        .maximum_force = 60000,
        .gain = 10000,
    };

    assert(force_effect_spring(&effect, 50) == 0);
    assert(force_effect_spring(&effect, 100) == 0);
    assert(force_effect_spring(&effect, 150) == 0);
    assert(force_effect_spring(&effect, 650) == -30000);
    assert(force_effect_spring(&effect, -450) == 30000);
}

static void test_spring_saturation_and_gain(void) {
    ForceSpringEffect effect = {
        .center = 0,
        .deadband = 0,
        .saturation_distance = 100,
        .maximum_force = UINT16_MAX,
        .gain = 2500,
    };

    assert(force_effect_spring(&effect, 50) == -8191);
    assert(force_effect_spring(&effect, 1000) == -16383);
    assert(force_effect_spring(&effect, -1000) == 16383);

    effect.saturation_distance = 0;
    assert(force_effect_spring(&effect, 1) == -16383);
}

static void test_damper(void) {
    const ForceDamperEffect effect = {
        .saturation_velocity = 2000,
        .maximum_force = 40000,
        .gain = 10000,
    };

    assert(force_effect_damper(&effect, 0) == 0);
    assert(force_effect_damper(&effect, 500) == -10000);
    assert(force_effect_damper(&effect, -1000) == 20000);
    assert(force_effect_damper(&effect, 3000) == -40000);
}

static void test_mixer(void) {
    assert(force_effect_mix(10000, 20000, 50000) == 30000);
    assert(force_effect_mix(40000, 20000, 50000) == 50000);
    assert(force_effect_mix(-40000, -20000, 50000) == -50000);
    assert(force_effect_mix(INT32_MAX, INT32_MAX, UINT16_MAX) == UINT16_MAX);
    assert(force_effect_mix(INT32_MIN, INT32_MIN, UINT16_MAX) == -(int32_t)UINT16_MAX);
}

int main(void) {
    test_constant_force();
    test_spring_deadband_and_direction();
    test_spring_saturation_and_gain();
    test_damper();
    test_mixer();
    return 0;
}
