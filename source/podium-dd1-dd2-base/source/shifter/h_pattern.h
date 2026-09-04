#ifndef OPENTEC_BASE_SHIFTER_H_PATTERN_H
#define OPENTEC_BASE_SHIFTER_H_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Identifies the currently selected H-pattern gear as a bit flag.
 *
 * Neutral is zero; every selected gear has one distinct bit so gear values can be carried in
 * protocol fields without ambiguity.
 */
typedef enum {
    SHIFTER_GEAR_NEUTRAL = 0,      /**< H-pattern neutral. */
    SHIFTER_GEAR_REVERSE = 1 << 0, /**< H-pattern reverse gear. */
    SHIFTER_GEAR_FIRST = 1 << 1,   /**< H-pattern first gear. */
    SHIFTER_GEAR_SECOND = 1 << 2,  /**< H-pattern second gear. */
    SHIFTER_GEAR_THIRD = 1 << 3,   /**< H-pattern third gear. */
    SHIFTER_GEAR_FOURTH = 1 << 4,  /**< H-pattern fourth gear. */
    SHIFTER_GEAR_FIFTH = 1 << 5,   /**< H-pattern fifth gear. */
    SHIFTER_GEAR_SIXTH = 1 << 6,   /**< H-pattern sixth gear. */
    SHIFTER_GEAR_SEVENTH = 1 << 7, /**< H-pattern seventh gear. */
} ShifterGear;

/**
 * @brief Stores the lateral and longitudinal H-pattern classification boundaries.
 *
 * Lateral boundaries separate neighboring gears, while row thresholds separate the upper and
 * lower gear rows from neutral.
 */
typedef struct {
    uint16_t reverse_first_boundary; /**< Boundary between reverse and first gear. */
    uint16_t first_third_boundary;   /**< Boundary between first and third gear. */
    uint16_t second_fourth_boundary; /**< Boundary between second and fourth gear. */
    uint16_t third_fifth_boundary;   /**< Boundary between third and fifth gear. */
    uint16_t fourth_sixth_boundary;  /**< Boundary between fourth and sixth gear. */
    uint16_t fifth_seventh_boundary; /**< Boundary between fifth and seventh gear. */
    uint16_t retained_boundary; /**< Persisted boundary slot not used by runtime classification. */
    uint16_t upper_row_threshold; /**< Longitudinal threshold for the reverse and odd-gear row. */
    uint16_t lower_row_threshold; /**< Longitudinal threshold for the even-gear row. */
} HPatternCalibration;

/**
 * @brief Stores H-pattern calibration and validity.
 *
 * The calibration values are usable only when calibrated is true.
 */
typedef struct {
    HPatternCalibration calibration; /**< Lateral and longitudinal H-pattern boundaries. */
    bool calibrated;                 /**< True when calibration contains usable thresholds. */
} HPatternSettings;

/**
 * @brief Retains H-pattern classification state between samples.
 *
 * The neutral and latched positions provide longitudinal hysteresis for the published gear.
 */
typedef struct {
    uint16_t neutral_position; /**< Last classified neutral longitudinal position. */
    uint16_t latched_position; /**< Longitudinal position associated with the current gear latch. */
    uint32_t
        update_deadline_ms; /**< Deadline after which another classification sample is allowed. */
    ShifterGear gear;       /**< Last published H-pattern gear. */
} HPatternShifter;

/**
 * @brief Reports whether H-pattern classification is due.
 *
 * The caller can use this gate to avoid sampling more often than the ten-millisecond interval.
 *
 * @param[in] shifter H-pattern timing state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the next classification sample is due.
 */
bool h_pattern_shifter_update_due(const HPatternShifter *shifter, uint32_t now_ms);

/**
 * @brief Selects the H-pattern gear value published to external consumers.
 *
 * Returns neutral while the H-pattern output is disabled without changing the retained
 * classifier state. This keeps the last valid gear available for resumption after a temporary
 * disconnect or calibration interval.
 *
 * @param[in] shifter Retained H-pattern classification state.
 * @param[in] enabled True when the classified gear may be published.
 * @return Retained gear when enabled and available; otherwise neutral.
 */
ShifterGear h_pattern_shifter_output_gear(const HPatternShifter *shifter, bool enabled);

/**
 * @brief Classifies one H-pattern analog sample.
 *
 * Applies the calibrated lateral boundaries and longitudinal hysteresis, updates the retained
 * state, and returns the published gear.
 *
 * @param[in,out] shifter Persistent H-pattern classification state.
 * @param[in] calibration Calibrated lateral boundaries and row thresholds.
 * @param[in] lateral_position Current lateral axis sample.
 * @param[in] longitudinal_position Current longitudinal axis sample.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Current or newly classified H-pattern gear.
 */
ShifterGear h_pattern_shifter_update(HPatternShifter *shifter,
                                     const HPatternCalibration *calibration,
                                     uint16_t lateral_position, uint16_t longitudinal_position,
                                     uint32_t now_ms);

#endif
