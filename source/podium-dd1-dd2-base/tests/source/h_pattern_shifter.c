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

static uint32_t now_ms;

static ShifterGear update(HPatternShifter *shifter, uint16_t lateral, uint16_t longitudinal) {
    now_ms += 11;
    return h_pattern_shifter_update(shifter, &calibration, lateral, longitudinal, now_ms);
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

static void test_reclassifies_after_strict_deadline(void) {
    HPatternShifter shifter = {0};

    assert(h_pattern_shifter_update(&shifter, &calibration, 801, 701, 0) == SHIFTER_GEAR_NEUTRAL);
    assert(h_pattern_shifter_update(&shifter, &calibration, 801, 701, 1) == SHIFTER_GEAR_REVERSE);
    shifter.neutral_position = 0;
    assert(h_pattern_shifter_update(&shifter, &calibration, 600, 701, 11) == SHIFTER_GEAR_REVERSE);
    assert(h_pattern_shifter_update(&shifter, &calibration, 600, 701, 12) == SHIFTER_GEAR_THIRD);
}

int main(void) {
    test_gear_map();
    test_row_hysteresis();
    test_neutral_hysteresis();
    test_reclassifies_after_strict_deadline();
    return 0;
}
