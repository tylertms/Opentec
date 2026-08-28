#include <assert.h>
#include <stdint.h>

#include "shifter/h_pattern.h"

static const HPatternCalibration calibration = {
    .reverse_first_boundary = 800,
    .first_third_boundary = 600,
    .second_fourth_boundary = 600,
    .third_fifth_boundary = 400,
    .fourth_sixth_boundary = 400,
    .fifth_seventh_boundary = 200,
    .upper_row_threshold = 700,
    .lower_row_threshold = 300,
};

static ShifterGear update(HPatternShifter *shifter, uint16_t lateral, uint16_t longitudinal) {
    return h_pattern_shifter_update(shifter, &calibration, lateral, longitudinal);
}

static HPatternCalibrationSamples calibration_samples(void) {
    return (HPatternCalibrationSamples){
        .neutral_longitudinal = 500,
        .reverse_lateral = 900,
        .reverse_longitudinal = 900,
        .first_lateral = 700,
        .second_lateral = 650,
        .second_longitudinal = 100,
        .third_lateral = 500,
        .fourth_lateral = 450,
        .fifth_lateral = 300,
        .sixth_lateral = 250,
        .seventh_lateral = 100,
    };
}

static void test_calibration_thresholds(void) {
    HPatternCalibrationSamples samples = calibration_samples();
    HPatternCalibration result = h_pattern_calibration_build(&samples);

    assert(result.reverse_first_boundary == 800);
    assert(result.first_third_boundary == 600);
    assert(result.second_fourth_boundary == 550);
    assert(result.third_fifth_boundary == 400);
    assert(result.fourth_sixth_boundary == 350);
    assert(result.fifth_seventh_boundary == 200);
    assert(result.upper_row_threshold == 700);
    assert(result.lower_row_threshold == 300);
}

static void test_seventh_gear_boundary_fallback(void) {
    HPatternCalibrationSamples samples = calibration_samples();
    samples.seventh_lateral = 295;
    assert(h_pattern_calibration_build(&samples).fifth_seventh_boundary == 280);

    samples.seventh_lateral = 294;
    assert(h_pattern_calibration_build(&samples).fifth_seventh_boundary == 297);
}

static void test_gear_map(void) {
    HPatternShifter shifter = {0};

    assert(update(&shifter, 801, 701) == SHIFTER_GEAR_REVERSE);
    shifter.neutral_position = 0;
    assert(update(&shifter, 800, 701) == SHIFTER_GEAR_FIRST);
    shifter.neutral_position = 0;
    assert(update(&shifter, 600, 701) == SHIFTER_GEAR_THIRD);
    shifter.neutral_position = 0;
    assert(update(&shifter, 400, 701) == SHIFTER_GEAR_FIFTH);
    shifter.neutral_position = 0;
    assert(update(&shifter, 200, 701) == SHIFTER_GEAR_SEVENTH);

    shifter.neutral_position = 0;
    assert(update(&shifter, 601, 299) == SHIFTER_GEAR_SECOND);
    shifter.neutral_position = 0;
    assert(update(&shifter, 600, 299) == SHIFTER_GEAR_FOURTH);
    shifter.neutral_position = 0;
    assert(update(&shifter, 400, 299) == SHIFTER_GEAR_SIXTH);

    shifter.neutral_position = 0;
    assert(update(&shifter, 500, 500) == SHIFTER_GEAR_NEUTRAL);
    assert(shifter.neutral_position == 500);
}

static void test_row_hysteresis(void) {
    HPatternShifter shifter = {
        .neutral_position = 500,
        .latched_position = 500,
        .gear = SHIFTER_GEAR_FIRST,
    };

    assert(update(&shifter, 500, 600) == SHIFTER_GEAR_FIRST);
    assert(shifter.latched_position == 500);
    assert(update(&shifter, 500, 601) == SHIFTER_GEAR_NEUTRAL);
    assert(shifter.neutral_position == 601);
    assert(shifter.latched_position == 601);

    shifter.neutral_position = 500;
    shifter.latched_position = 500;
    shifter.gear = SHIFTER_GEAR_SECOND;
    assert(update(&shifter, 500, 400) == SHIFTER_GEAR_SECOND);
    assert(update(&shifter, 500, 399) == SHIFTER_GEAR_NEUTRAL);
}

static void test_neutral_hysteresis(void) {
    HPatternShifter shifter = {
        .neutral_position = 500,
        .latched_position = 500,
        .gear = SHIFTER_GEAR_NEUTRAL,
    };

    assert(update(&shifter, 900, 600) == SHIFTER_GEAR_NEUTRAL);
    assert(update(&shifter, 900, 601) == SHIFTER_GEAR_NEUTRAL);
    assert(shifter.neutral_position == 601);
}

int main(void) {
    test_calibration_thresholds();
    test_seventh_gear_boundary_fallback();
    test_gear_map();
    test_row_hysteresis();
    test_neutral_hysteresis();
    return 0;
}
