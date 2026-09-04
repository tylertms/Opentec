#include "shifter/h_pattern.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief H-pattern classification timing constants.
 */
enum { H_PATTERN_UPDATE_INTERVAL_MS = 10 /**< Minimum interval between classification samples. */ };

/**
 * @brief Identifies gears in the upper H-pattern row.
 *
 * The upper row contains reverse and the odd-numbered forward gears.
 *
 * @param[in] gear Gear code to inspect.
 * @return True when the gear belongs to the upper row.
 */
static bool is_upper_row_gear(ShifterGear gear) {
    return gear == SHIFTER_GEAR_REVERSE || gear == SHIFTER_GEAR_FIRST ||
           gear == SHIFTER_GEAR_THIRD || gear == SHIFTER_GEAR_FIFTH || gear == SHIFTER_GEAR_SEVENTH;
}

/**
 * @brief Identifies gears in the lower H-pattern row.
 *
 * The lower row contains the even-numbered forward gears.
 *
 * @param[in] gear Gear code to inspect.
 * @return True when the gear belongs to the lower row.
 */
static bool is_lower_row_gear(ShifterGear gear) {
    return gear == SHIFTER_GEAR_SECOND || gear == SHIFTER_GEAR_FOURTH || gear == SHIFTER_GEAR_SIXTH;
}

/**
 * @brief Retains a gear while longitudinal movement remains inside its row allowance.
 *
 * The allowance is half the distance from the captured neutral reference to the applicable row
 * threshold. Neutral remains latched only while both row allowances hold.
 *
 * @param[in] shifter Current gear, neutral reference, and last accepted longitudinal position.
 * @param[in] calibration Calibrated upper and lower row thresholds.
 * @param[in] longitudinal_position Current longitudinal axis sample.
 * @return True when the current gear remains latched.
 */
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

/**
 * @brief Selects an upper-row gear from the lateral axis.
 *
 * Applies the reverse-first, first-third, third-fifth, and fifth-seventh boundaries in order.
 *
 * @param[in] calibration Calibrated upper-row boundaries.
 * @param[in] lateral_position Current lateral axis sample.
 * @return Reverse or the selected odd-numbered forward gear.
 */
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

/**
 * @brief Selects a lower-row gear from the lateral axis.
 *
 * Applies the second-fourth and fourth-sixth boundaries in order.
 *
 * @param[in] calibration Calibrated lower-row boundaries.
 * @param[in] lateral_position Current lateral axis sample.
 * @return Second, fourth, or sixth gear.
 */
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
 * @brief Reports whether the next H-pattern classification sample is due.
 *
 * Opens the sampling boundary only after the strict ten-millisecond deadline has passed.
 *
 * @param[in] shifter H-pattern timing state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the caller should acquire and classify a new sample.
 */
bool h_pattern_shifter_update_due(const HPatternShifter *shifter, uint32_t now_ms) {
    return (int32_t)(now_ms - shifter->update_deadline_ms) > 0;
}

/**
 * @brief Selects the H-pattern gear value published to external consumers.
 *
 * Temporarily neutralizes the published output while retaining the classifier's hysteresis and
 * timing state for the next active H-pattern sample.
 *
 * @param[in] shifter Retained H-pattern classification state.
 * @param[in] enabled True when the classified gear may be published.
 * @return Retained gear when enabled and available; otherwise neutral.
 */
ShifterGear h_pattern_shifter_output_gear(const HPatternShifter *shifter, bool enabled) {
    return enabled && shifter != NULL ? shifter->gear : SHIFTER_GEAR_NEUTRAL;
}

/**
 * @brief Classifies one calibrated H-pattern shifter sample with row hysteresis.
 *
 * Reclassifies after each strict ten-millisecond sampling deadline. Between deadlines it retains
 * the published gear. An eligible sample retains the previous gear inside its longitudinal
 * allowance, otherwise selects the active row and applies its lateral gear boundaries. A sample
 * between the row thresholds updates the neutral reference when it is not retained as latched.
 *
 * @param[in,out] shifter Persistent neutral reference, last accepted row position, and gear.
 * @param[in] calibration Ordered lateral gear boundaries and longitudinal row thresholds.
 * @param[in] lateral_position Current lateral axis sample.
 * @param[in] longitudinal_position Current longitudinal axis sample.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Latched or newly classified neutral, reverse, or forward gear.
 */
ShifterGear h_pattern_shifter_update(HPatternShifter *shifter,
                                     const HPatternCalibration *calibration,
                                     uint16_t lateral_position, uint16_t longitudinal_position,
                                     uint32_t now_ms) {
    if (!h_pattern_shifter_update_due(shifter, now_ms)) {
        return shifter->gear;
    }
    shifter->update_deadline_ms = now_ms + H_PATTERN_UPDATE_INTERVAL_MS;

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
