#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "cooling/controller.h"

static void test_standard_fan_profile(void) {
    CoolingController controller;
    cooling_controller_init(&controller, false);
    assert(controller.primary_duty_percent == 25);
    assert(controller.secondary_duty_percent == 25);
    assert(controller.force_scale_percent == 100);
    assert(controller.phase == COOLING_PHASE_INITIALIZE);
    assert(controller.low_threshold_offset == 5);
    assert(controller.high_threshold_offset == 4);
    assert(controller.primary_delay_ms == -30000);
    assert(controller.secondary_delay_ms == -120000);

    cooling_controller_update(&controller, 20.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_IDLE);
    assert(controller.primary_duty_percent == 100);
    assert(controller.secondary_duty_percent == 0);

    cooling_controller_update(&controller, 20.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_IDLE);
    assert(controller.primary_duty_percent == 0);
    assert(controller.secondary_duty_percent == 0);

    cooling_controller_update(&controller, 36.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_LOW);
    assert(controller.primary_duty_percent == 0);
    cooling_controller_update(&controller, 46.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_MEDIUM);
    assert(controller.primary_duty_percent == 20);
    cooling_controller_update(&controller, 61.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_HIGH);
    assert(controller.primary_duty_percent == 40);
    cooling_controller_update(&controller, 76.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_NEAR_MAXIMUM);
    assert(controller.primary_duty_percent == 50);
    cooling_controller_update(&controller, 96.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_FULL);
    assert(controller.primary_duty_percent == 70);
}

static void test_standard_hysteresis_and_limit(void) {
    CoolingController controller;
    cooling_controller_init(&controller, false);
    controller.phase = COOLING_PHASE_FULL;

    cooling_controller_update(&controller, 121.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_STANDARD_LIMIT);
    assert(controller.primary_duty_percent == 100);
    assert(controller.force_scale_percent == 0);

    cooling_controller_update(&controller, 115.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_STANDARD_LIMIT);
    cooling_controller_update(&controller, 114.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_FULL);
    assert(controller.force_scale_percent == 100);
}

static void test_dual_fan_profile(void) {
    CoolingController controller;
    cooling_controller_init(&controller, true);
    controller.phase = COOLING_PHASE_LOW;
    cooling_controller_update(&controller, 40.0f, true, false, 0);
    assert(controller.primary_duty_percent == 5);
    assert(controller.secondary_duty_percent == 0);
    controller.phase = COOLING_PHASE_MEDIUM;
    cooling_controller_update(&controller, 50.0f, true, false, 0);
    assert(controller.primary_duty_percent == 5);
    assert(controller.secondary_duty_percent == 4);
    controller.phase = COOLING_PHASE_NEAR_MAXIMUM;
    cooling_controller_update(&controller, 80.0f, true, false, 0);
    assert(controller.primary_duty_percent == 6);
    assert(controller.secondary_duty_percent == 6);
}

static void test_managed_window(void) {
    CoolingController controller;
    cooling_controller_init(&controller, true);
    controller.phase = COOLING_PHASE_FULL;

    cooling_controller_update(&controller, 131.0f, true, false, 100);
    assert(controller.phase == COOLING_PHASE_START_MANAGED_WINDOW);
    assert(controller.force_scale_percent == 70);
    cooling_controller_update(&controller, 131.0f, true, false, 100);
    assert(controller.phase == COOLING_PHASE_MANAGED_WINDOW);
    assert(controller.primary_deadline_ms == 210100);
    assert(controller.secondary_deadline_ms == 300100);
    cooling_controller_update(&controller, 140.0f, true, false, 101);
    assert(controller.phase == COOLING_PHASE_MANAGED_LIMIT);
    assert(controller.force_scale_percent == 0);
    cooling_controller_update(&controller, 129.0f, true, false, 102);
    assert(controller.phase == COOLING_PHASE_FULL);
    assert(controller.force_scale_percent == 80);
}

static void test_configuration_limits(void) {
    CoolingController controller;
    cooling_controller_init(&controller, false);
    cooling_controller_set_low_threshold_offset(&controller, -5);
    cooling_controller_set_high_threshold_offset(&controller, 5);
    cooling_controller_set_primary_delay_seconds(&controller, -120);
    cooling_controller_set_secondary_delay_seconds(&controller, 120);
    assert(controller.low_threshold_offset == -5);
    assert(controller.high_threshold_offset == 5);
    assert(controller.primary_delay_ms == -120000);
    assert(controller.secondary_delay_ms == 120000);

    cooling_controller_set_low_threshold_offset(&controller, -6);
    cooling_controller_set_high_threshold_offset(&controller, 6);
    cooling_controller_set_primary_delay_seconds(&controller, -121);
    cooling_controller_set_secondary_delay_seconds(&controller, 121);
    assert(controller.low_threshold_offset == -5);
    assert(controller.high_threshold_offset == 5);
    assert(controller.primary_delay_ms == -120000);
    assert(controller.secondary_delay_ms == 120000);
}

static void test_suspend_and_output_inhibit(void) {
    CoolingController controller;
    cooling_controller_init(&controller, false);
    cooling_controller_set_suspend_request(&controller, UINT8_MAX);
    cooling_controller_update(&controller, 100.0f, false, false, 0);
    assert(controller.phase == COOLING_PHASE_INITIALIZE);
    assert(controller.primary_duty_percent == 25);

    cooling_controller_set_suspend_request(&controller, 0);
    cooling_controller_update(&controller, 100.0f, false, true, 0);
    assert(controller.phase == COOLING_PHASE_IDLE);
    assert(controller.primary_duty_percent == 100);
    assert(controller.force_scale_percent == 0);

    cooling_controller_update(&controller, 100.0f, false, true, 0);
    assert(controller.phase == COOLING_PHASE_LOW);
    assert(controller.primary_duty_percent == 0);
    assert(controller.force_scale_percent == 0);

    cooling_controller_update(&controller, 100.0f, false, true, 0);
    assert(controller.phase == COOLING_PHASE_MEDIUM);
    assert(controller.force_scale_percent == 0);
}

static void test_service_override(void) {
    CoolingController controller;
    cooling_controller_init(&controller, true);

    cooling_controller_apply_service_override(&controller, UINT8_MAX, 25, 75, 120);
    assert(controller.automatic_control_suspended);
    assert(controller.primary_duty_percent == 25);
    assert(controller.secondary_duty_percent == 75);
    assert(controller.force_scale_percent == 100);

    cooling_controller_apply_service_override(&controller, 0, 1, 2, 3);
    assert(!controller.automatic_control_suspended);
    assert(controller.primary_duty_percent == 25);
    assert(controller.secondary_duty_percent == 75);
    assert(controller.force_scale_percent == 100);
}

int main(void) {
    test_standard_fan_profile();
    test_standard_hysteresis_and_limit();
    test_dual_fan_profile();
    test_managed_window();
    test_configuration_limits();
    test_suspend_and_output_inhibit();
    test_service_override();
    return 0;
}
