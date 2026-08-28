#include "analog/auxiliary_axis.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static AuxiliaryAxis initialized_axis(uint16_t minimum, uint16_t maximum) {
    AuxiliaryAxis axis;
    AuxiliaryAxisSettings settings = {
        .minimum = minimum,
        .maximum = maximum,
        .reset_on_start = false,
    };
    auxiliary_axis_init(&axis, &settings);
    return axis;
}

static void test_detects_the_unfiltered_connection_threshold(void) {
    AuxiliaryAxis axis = initialized_axis(100, 1100);

    AuxiliaryAxisReading disconnected =
        auxiliary_axis_update(&axis, 61, AUXILIARY_AXIS_MANUAL_CALIBRATION, 0);
    assert(!disconnected.active);
    assert(disconnected.value == 0);

    AuxiliaryAxisReading connected =
        auxiliary_axis_update(&axis, 62, AUXILIARY_AXIS_MANUAL_CALIBRATION, 1);
    assert(connected.active);
}

static void test_filters_and_scales_the_normalized_sample(void) {
    AuxiliaryAxis axis = initialized_axis(100, 1100);

    AuxiliaryAxisReading first =
        auxiliary_axis_update(&axis, 598, AUXILIARY_AXIS_MANUAL_CALIBRATION, 0);
    assert(first.active);
    assert(first.value == 127);

    AuxiliaryAxisReading inside_deadband =
        auxiliary_axis_update(&axis, 604, AUXILIARY_AXIS_MANUAL_CALIBRATION, 1);
    assert(inside_deadband.value == 127);

    AuxiliaryAxisReading averaged =
        auxiliary_axis_update(&axis, 620, AUXILIARY_AXIS_MANUAL_CALIBRATION, 2);
    assert(averaged.value == 130);
}

static void test_learns_the_minimum_from_ten_samples(void) {
    AuxiliaryAxisSettings settings;
    auxiliary_axis_settings_defaults(&settings);
    AuxiliaryAxis axis;
    auxiliary_axis_init(&axis, &settings);

    for (uint8_t sample = 0; sample < 10; sample++) {
        (void)auxiliary_axis_update(&axis, 98, AUXILIARY_AXIS_MANUAL_CALIBRATION, sample);
    }

    assert(!axis.learning_minimum);
    assert(axis.settings.minimum == 140);
    assert(axis.settle_threshold == 340);
    assert(axis.minimum_sample_count == 0);
}

static void test_manual_endpoint_captures_apply_margins(void) {
    AuxiliaryAxis minimum_axis = initialized_axis(100, 1000);
    auxiliary_axis_request_adjustment(&minimum_axis, AUXILIARY_AXIS_ADJUST_MINIMUM);
    (void)auxiliary_axis_update(&minimum_axis, 198, AUXILIARY_AXIS_MANUAL_CALIBRATION, 0);
    assert(minimum_axis.settings.minimum == 240);

    AuxiliaryAxis maximum_axis = initialized_axis(100, 1000);
    auxiliary_axis_request_adjustment(&maximum_axis, AUXILIARY_AXIS_ADJUST_MAXIMUM);
    (void)auxiliary_axis_update(&maximum_axis, 798, AUXILIARY_AXIS_MANUAL_CALIBRATION, 0);
    assert(maximum_axis.settings.maximum == 760);
    assert(!maximum_axis.limits_uninitialized);
}

static void test_automatic_maximum_requires_a_two_second_hold(void) {
    AuxiliaryAxisSettings settings;
    auxiliary_axis_settings_defaults(&settings);
    AuxiliaryAxis axis;
    auxiliary_axis_init(&axis, &settings);
    AuxiliaryAxisSettings snapshot;
    assert(auxiliary_axis_take_settings(&axis, AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, &snapshot));

    for (uint8_t sample = 0; sample < 10; sample++) {
        (void)auxiliary_axis_update(&axis, 98, AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, sample);
    }
    (void)auxiliary_axis_update(&axis, 998, AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, 0);
    assert(axis.settling);
    assert(axis.settings.maximum == 550);

    (void)auxiliary_axis_update(&axis, 998, AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, 1999);
    assert(axis.limits_uninitialized);
    (void)auxiliary_axis_update(&axis, 998, AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, 2000);

    assert(!axis.settling);
    assert(!axis.limits_uninitialized);
    assert(axis.settings.maximum == 735);
    assert(auxiliary_axis_take_settings(&axis, AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, &snapshot));
    assert(snapshot.minimum == 140);
    assert(snapshot.maximum == 735);
    assert(snapshot.reset_on_start);
}

static void test_settings_snapshot_uses_the_current_mode(void) {
    AuxiliaryAxis axis = initialized_axis(100, 1000);
    AuxiliaryAxisSettings snapshot;

    assert(!auxiliary_axis_take_settings(&axis, AUXILIARY_AXIS_MANUAL_CALIBRATION, &snapshot));
    auxiliary_axis_request_adjustment(&axis, AUXILIARY_AXIS_ADJUST_MINIMUM);
    assert(auxiliary_axis_take_settings(&axis, AUXILIARY_AXIS_MANUAL_CALIBRATION, &snapshot));
    assert(!snapshot.reset_on_start);
    assert(!auxiliary_axis_take_settings(&axis, AUXILIARY_AXIS_MANUAL_CALIBRATION, &snapshot));
}

static void test_reset_restarts_endpoint_learning(void) {
    AuxiliaryAxisSettings settings;
    auxiliary_axis_settings_defaults(&settings);
    AuxiliaryAxis axis;
    auxiliary_axis_init(&axis, &settings);

    (void)auxiliary_axis_update(&axis, 98, AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, 0);
    axis.settling = true;
    axis.settle_deadline_ms = 2000;
    auxiliary_axis_request_adjustment(&axis, AUXILIARY_AXIS_ADJUST_MAXIMUM);
    auxiliary_axis_reset(&axis);

    assert(axis.minimum_sample_count == 0);
    assert(axis.settle_deadline_ms == 0);
    assert(!axis.settling);
    assert(!axis.maximum_adjustment_pending);
    assert(axis.learning_minimum);
}

int main(void) {
    test_detects_the_unfiltered_connection_threshold();
    test_filters_and_scales_the_normalized_sample();
    test_learns_the_minimum_from_ten_samples();
    test_manual_endpoint_captures_apply_margins();
    test_automatic_maximum_requires_a_two_second_hold();
    test_settings_snapshot_uses_the_current_mode();
    test_reset_restarts_endpoint_learning();
    return 0;
}
