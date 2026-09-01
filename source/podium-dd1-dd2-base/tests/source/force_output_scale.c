#include <assert.h>
#include <stdint.h>

#include "force_feedback/output_scale.h"

static ForceOutputScale full_scale(void) {
    return (ForceOutputScale){
        .available_percent = 100,
        .tuning_strength_percent = 100,
        .output_strength_percent = 100,
    };
}

static void test_splits_and_clamps_primary_force(void) {
    ForceOutputScale scale = full_scale();
    ForceOutputReport report = {0};

    force_output_scale_apply(-70000, 500, scale, &report);
    assert(!report.positive_direction);
    assert(report.primary_magnitude == UINT16_MAX);
    assert(report.secondary_magnitude == 500);

    force_output_scale_apply(0, 0, scale, &report);
    assert(report.positive_direction);
    assert(report.primary_magnitude == 0);
}

static void test_applies_all_strength_percentages(void) {
    ForceOutputScale scale = {
        .available_percent = 50,
        .tuning_strength_percent = 80,
        .output_strength_percent = 40,
    };
    ForceOutputReport report = {0};

    force_output_scale_apply(50000, 0, scale, &report);
    assert(report.primary_magnitude == 10485);
    assert(report.secondary_magnitude == 0);
}

static void test_secondary_limit_depends_on_primary_limit(void) {
    ForceOutputScale scale = {
        .available_percent = 50,
        .tuning_strength_percent = 80,
        .output_strength_percent = 40,
    };
    ForceOutputReport report = {0};

    force_output_scale_apply(10000, 40000, scale, &report);
    assert(report.primary_magnitude == 4000);
    assert(report.secondary_magnitude == 10485);

    force_output_scale_apply(40000, 50000, scale, &report);
    assert(report.primary_magnitude == 10485);
    assert(report.secondary_magnitude == 20000);
}

static void test_disabled_secondary_output_is_preserved(void) {
    ForceOutputScale scale = full_scale();
    scale.secondary_output_disabled = true;
    ForceOutputReport report = {.secondary_magnitude = 0x4321};

    force_output_scale_apply(1234, 5678, scale, &report);
    assert(report.primary_magnitude == 1234);
    assert(report.secondary_magnitude == 0x4321);
}

static void test_available_force_stays_wide_through_tuning_scale(void) {
    ForceOutputScale scale = {
        .available_percent = 200,
        .tuning_strength_percent = 50,
        .output_strength_percent = 100,
    };
    ForceOutputReport report = {0};

    force_output_scale_apply(INT32_MAX, 0, scale, &report);
    assert(report.primary_magnitude == UINT16_MAX);

    scale.available_percent = 100;
    scale.tuning_strength_percent = 101;
    force_output_scale_apply(50000, 0, scale, &report);
    assert(report.primary_magnitude == 50000);
}

int main(void) {
    test_splits_and_clamps_primary_force();
    test_applies_all_strength_percentages();
    test_secondary_limit_depends_on_primary_limit();
    test_disabled_secondary_output_is_preserved();
    test_available_force_stays_wide_through_tuning_scale();
    return 0;
}
