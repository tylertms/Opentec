#ifndef OPENTEC_BASE_SHIFTER_H_PATTERN_H
#define OPENTEC_BASE_SHIFTER_H_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SHIFTER_GEAR_NEUTRAL = 0,
    SHIFTER_GEAR_REVERSE = 1 << 0,
    SHIFTER_GEAR_FIRST = 1 << 1,
    SHIFTER_GEAR_SECOND = 1 << 2,
    SHIFTER_GEAR_THIRD = 1 << 3,
    SHIFTER_GEAR_FOURTH = 1 << 4,
    SHIFTER_GEAR_FIFTH = 1 << 5,
    SHIFTER_GEAR_SIXTH = 1 << 6,
    SHIFTER_GEAR_SEVENTH = 1 << 7,
} ShifterGear;

typedef struct {
    uint16_t reverse_first_boundary;
    uint16_t first_third_boundary;
    uint16_t second_fourth_boundary;
    uint16_t third_fifth_boundary;
    uint16_t fourth_sixth_boundary;
    uint16_t fifth_seventh_boundary;
    uint16_t upper_row_threshold;
    uint16_t lower_row_threshold;
} HPatternCalibration;

typedef struct {
    HPatternCalibration calibration;
    bool calibrated;
} HPatternSettings;

typedef struct {
    uint16_t neutral_longitudinal;
    uint16_t reverse_lateral;
    uint16_t reverse_longitudinal;
    uint16_t first_lateral;
    uint16_t second_lateral;
    uint16_t second_longitudinal;
    uint16_t third_lateral;
    uint16_t fourth_lateral;
    uint16_t fifth_lateral;
    uint16_t sixth_lateral;
    uint16_t seventh_lateral;
} HPatternCalibrationSamples;

typedef struct {
    uint16_t neutral_position;
    uint16_t latched_position;
    ShifterGear gear;
} HPatternShifter;

HPatternCalibration h_pattern_calibration_build(const HPatternCalibrationSamples *samples);
ShifterGear h_pattern_shifter_update(HPatternShifter *shifter,
                                     const HPatternCalibration *calibration,
                                     uint16_t lateral_position, uint16_t longitudinal_position);

#endif
