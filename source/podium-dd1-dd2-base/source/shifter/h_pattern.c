#include "shifter/h_pattern.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    SEVENTH_BOUNDARY_MINIMUM_SPAN = 5,
    SEVENTH_BOUNDARY_FALLBACK_OFFSET = 20,
};

static uint16_t midpoint(uint16_t first, uint16_t second) {
    uint16_t sum = first + second;
    return sum >> 1;
}

/**
 * Derives the H-pattern row thresholds and lateral gear boundaries from calibration samples.
 *
 * @param samples Neutral, reverse, and seven forward-gear axis samples.
 * @return Calibration thresholds used to classify subsequent shifter positions.
 */
HPatternCalibration h_pattern_calibration_build(const HPatternCalibrationSamples *samples) {
    uint16_t fifth_seventh_span = samples->fifth_lateral - samples->seventh_lateral;
    uint16_t fifth_seventh_boundary =
        fifth_seventh_span <= SEVENTH_BOUNDARY_MINIMUM_SPAN
            ? samples->fifth_lateral - SEVENTH_BOUNDARY_FALLBACK_OFFSET
            : midpoint(samples->fifth_lateral, samples->seventh_lateral);

    return (HPatternCalibration){
        .reverse_first_boundary = midpoint(samples->reverse_lateral, samples->first_lateral),
        .first_third_boundary = midpoint(samples->first_lateral, samples->third_lateral),
        .second_fourth_boundary = midpoint(samples->second_lateral, samples->fourth_lateral),
        .third_fifth_boundary = midpoint(samples->third_lateral, samples->fifth_lateral),
        .fourth_sixth_boundary = midpoint(samples->fourth_lateral, samples->sixth_lateral),
        .fifth_seventh_boundary = fifth_seventh_boundary,
        .upper_row_threshold =
            midpoint(samples->neutral_longitudinal, samples->reverse_longitudinal),
        .lower_row_threshold =
            midpoint(samples->neutral_longitudinal, samples->second_longitudinal),
    };
}

static bool is_upper_row_gear(ShifterGear gear) {
    return gear == SHIFTER_GEAR_REVERSE || gear == SHIFTER_GEAR_FIRST ||
           gear == SHIFTER_GEAR_THIRD || gear == SHIFTER_GEAR_FIFTH || gear == SHIFTER_GEAR_SEVENTH;
}

static bool is_lower_row_gear(ShifterGear gear) {
    return gear == SHIFTER_GEAR_SECOND || gear == SHIFTER_GEAR_FOURTH || gear == SHIFTER_GEAR_SIXTH;
}

static bool gear_remains_latched(const HPatternShifter *shifter,
                                 const HPatternCalibration *calibration,
                                 uint16_t longitudinal_position) {
    if (shifter->neutral_position == 0) {
        return false;
    }

    uint16_t movement = longitudinal_position > shifter->latched_position
                            ? longitudinal_position - shifter->latched_position
                            : shifter->latched_position - longitudinal_position;
    bool within_upper =
        calibration->upper_row_threshold > shifter->neutral_position &&
        movement <= (calibration->upper_row_threshold - shifter->neutral_position) / 2;
    bool within_lower =
        shifter->neutral_position > calibration->lower_row_threshold &&
        movement <= (shifter->neutral_position - calibration->lower_row_threshold) / 2;

    if (is_upper_row_gear(shifter->gear)) {
        return within_upper;
    }
    if (is_lower_row_gear(shifter->gear)) {
        return within_lower;
    }
    return shifter->gear == SHIFTER_GEAR_NEUTRAL && within_upper && within_lower;
}

static ShifterGear select_upper_row(const HPatternCalibration *calibration,
                                    uint16_t lateral_position) {
    if (lateral_position > calibration->reverse_first_boundary) {
        return SHIFTER_GEAR_REVERSE;
    }
    if (lateral_position > calibration->first_third_boundary) {
        return SHIFTER_GEAR_FIRST;
    }
    if (lateral_position > calibration->third_fifth_boundary) {
        return SHIFTER_GEAR_THIRD;
    }
    if (lateral_position > calibration->fifth_seventh_boundary) {
        return SHIFTER_GEAR_FIFTH;
    }
    return SHIFTER_GEAR_SEVENTH;
}

static ShifterGear select_lower_row(const HPatternCalibration *calibration,
                                    uint16_t lateral_position) {
    if (lateral_position > calibration->second_fourth_boundary) {
        return SHIFTER_GEAR_SECOND;
    }
    if (lateral_position > calibration->fourth_sixth_boundary) {
        return SHIFTER_GEAR_FOURTH;
    }
    return SHIFTER_GEAR_SIXTH;
}

/**
 * Classifies one calibrated H-pattern shifter sample with row hysteresis.
 *
 * @param shifter Persistent neutral reference, last accepted row position, and gear.
 * @param calibration Ordered lateral gear boundaries and longitudinal row thresholds.
 * @param lateral_position Current lateral axis sample.
 * @param longitudinal_position Current longitudinal axis sample.
 * @return Latched or newly classified neutral, reverse, or forward gear.
 */
ShifterGear h_pattern_shifter_update(HPatternShifter *shifter,
                                     const HPatternCalibration *calibration,
                                     uint16_t lateral_position, uint16_t longitudinal_position) {
    if (gear_remains_latched(shifter, calibration, longitudinal_position)) {
        return shifter->gear;
    }

    if (longitudinal_position > calibration->upper_row_threshold) {
        shifter->gear = select_upper_row(calibration, lateral_position);
    } else if (longitudinal_position < calibration->lower_row_threshold) {
        shifter->gear = select_lower_row(calibration, lateral_position);
    } else {
        shifter->gear = SHIFTER_GEAR_NEUTRAL;
        shifter->neutral_position = longitudinal_position;
    }

    shifter->latched_position = longitudinal_position;
    return shifter->gear;
}
