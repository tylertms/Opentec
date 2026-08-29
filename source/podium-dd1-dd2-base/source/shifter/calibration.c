#include "shifter/calibration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    H_PATTERN_CALIBRATION_OPCODE = 0x19,
    H_PATTERN_CALIBRATION_START_SELECTOR = 1,
    H_PATTERN_CALIBRATION_ADVANCE_SELECTOR = 2,
    SEVENTH_BOUNDARY_MINIMUM_SPAN = 5,
    SEVENTH_BOUNDARY_FALLBACK_OFFSET = 20,
};

/**
 * @brief Computes the midpoint of two axis samples.
 *
 * Adds both samples with 16-bit wrapping and divides the result by two.
 *
 * @param[in] first First axis sample.
 * @param[in] second Second axis sample.
 * @return Integer midpoint of the samples.
 */
static uint16_t midpoint(uint16_t first, uint16_t second) {
    uint16_t sum = first + second;
    return sum >> 1;
}

/**
 * @brief Decodes an H-pattern calibration command.
 *
 * Accepts operating-mode opcode 0x19 with selector one for start or selector two for advance.
 *
 * @param[in] source Decoded F8 09 operating-mode command.
 * @param[out] command Destination for the calibration action.
 * @return True when the opcode and selector identify a supported calibration command.
 */
bool h_pattern_calibration_command_decode(const UsbOperatingModeCommand *source,
                                          HPatternCalibrationCommand *command) {
    if (source == NULL || command == NULL || source->opcode != H_PATTERN_CALIBRATION_OPCODE) {
        return false;
    }

    switch (source->parameters[0]) {
    case H_PATTERN_CALIBRATION_START_SELECTOR:
        *command = H_PATTERN_CALIBRATION_COMMAND_START;
        return true;
    case H_PATTERN_CALIBRATION_ADVANCE_SELECTOR:
        *command = H_PATTERN_CALIBRATION_COMMAND_ADVANCE;
        return true;
    default:
        return false;
    }
}

/**
 * @brief Derives H-pattern thresholds from captured positions.
 *
 * Uses adjacent-gear midpoints for the six lateral boundaries and neutral-to-row midpoints for
 * the two longitudinal thresholds. When fifth and seventh gear are no more than five counts apart,
 * the final boundary is placed twenty counts below fifth gear.
 *
 * @param[in] samples Neutral, reverse, and seven forward-gear axis samples.
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

/**
 * @brief Captures one position in the H-pattern calibration sequence.
 *
 * Stores neutral, reverse, and gears one through seven in order. The seventh capture builds and
 * enables the completed calibration.
 *
 * @param[in,out] session Current position and collected samples.
 * @param[in] lateral_position Current lateral axis sample.
 * @param[in] longitudinal_position Current longitudinal axis sample.
 * @param[in,out] settings Destination for the completed calibration.
 * @return No capture after completion, completed on seventh gear, otherwise captured.
 */
static HPatternCalibrationResult capture_position(HPatternCalibrationSession *session,
                                                  uint16_t lateral_position,
                                                  uint16_t longitudinal_position,
                                                  HPatternSettings *settings) {
    switch (session->next_position) {
    case H_PATTERN_CALIBRATION_NEUTRAL:
        session->samples.neutral_longitudinal = longitudinal_position;
        break;
    case H_PATTERN_CALIBRATION_REVERSE:
        session->samples.reverse_lateral = lateral_position;
        session->samples.reverse_longitudinal = longitudinal_position;
        break;
    case H_PATTERN_CALIBRATION_FIRST:
        session->samples.first_lateral = lateral_position;
        break;
    case H_PATTERN_CALIBRATION_SECOND:
        session->samples.second_lateral = lateral_position;
        session->samples.second_longitudinal = longitudinal_position;
        break;
    case H_PATTERN_CALIBRATION_THIRD:
        session->samples.third_lateral = lateral_position;
        break;
    case H_PATTERN_CALIBRATION_FOURTH:
        session->samples.fourth_lateral = lateral_position;
        break;
    case H_PATTERN_CALIBRATION_FIFTH:
        session->samples.fifth_lateral = lateral_position;
        break;
    case H_PATTERN_CALIBRATION_SIXTH:
        session->samples.sixth_lateral = lateral_position;
        break;
    case H_PATTERN_CALIBRATION_SEVENTH:
        session->samples.seventh_lateral = lateral_position;
        settings->calibration = h_pattern_calibration_build(&session->samples);
        settings->calibrated = true;
        session->next_position = H_PATTERN_CALIBRATION_COMPLETE;
        return H_PATTERN_CALIBRATION_COMPLETED;
    case H_PATTERN_CALIBRATION_COMPLETE:
        return H_PATTERN_CALIBRATION_NO_CAPTURE;
    }

    session->next_position++;
    return H_PATTERN_CALIBRATION_CAPTURED;
}

/**
 * @brief Applies a calibration lifecycle command.
 *
 * Start clears the collected samples and opens a new session. Advance queues one capture for the
 * next available H-pattern analog sample.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] command Requested start or advance action.
 */
void h_pattern_calibration_service_request(HPatternCalibrationService *service,
                                           HPatternCalibrationCommand command) {
    if (service == NULL) {
        return;
    }

    if (command == H_PATTERN_CALIBRATION_COMMAND_START) {
        *service = (HPatternCalibrationService){.active = true};
    } else if (command == H_PATTERN_CALIBRATION_COMMAND_ADVANCE) {
        service->advance_pending = true;
    }
}

/**
 * @brief Starts calibration for an uninitialized H-pattern input.
 *
 * Opens a fresh capture session when an H-pattern shifter is available without saved calibration.
 * An active session or calibrated input remains unchanged.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] input_available True when either shifter input is in H-pattern mode.
 * @param[in] calibrated True when usable H-pattern thresholds are already available.
 * @return True when a new calibration session was started.
 */
bool h_pattern_calibration_service_start_if_required(HPatternCalibrationService *service,
                                                     bool input_available, bool calibrated) {
    if (service == NULL || service->active || !input_available || calibrated) {
        return false;
    }

    *service = (HPatternCalibrationService){.active = true};
    return true;
}

/**
 * @brief Updates the attached-wheel calibration-advance input.
 *
 * Retains the current input level so a captured position must be followed by a release before the
 * next physical or queued host advance can capture another position.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] active True while the attached-wheel calibration input is pressed.
 */
void h_pattern_calibration_service_set_advance_input(HPatternCalibrationService *service,
                                                     bool active) {
    if (service != NULL) {
        service->advance_input_active = active;
    }
}

/**
 * @brief Captures a queued H-pattern calibration position.
 *
 * Ignores samples until a session is active and either the attached-wheel input or a host advance
 * is active. After each capture, waits for the attached-wheel input to be released before accepting
 * the next position. A successful seventh-gear capture closes the session and enables the new
 * settings.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] lateral_position Current lateral axis sample.
 * @param[in] longitudinal_position Current longitudinal axis sample.
 * @param[in,out] settings Destination for the completed calibration.
 * @return No capture, an intermediate capture, or completed calibration.
 */
HPatternCalibrationResult h_pattern_calibration_service_capture(HPatternCalibrationService *service,
                                                                uint16_t lateral_position,
                                                                uint16_t longitudinal_position,
                                                                HPatternSettings *settings) {
    if (service == NULL || settings == NULL || !service->active) {
        return H_PATTERN_CALIBRATION_NO_CAPTURE;
    }

    if (service->release_required) {
        if (!service->advance_input_active) {
            service->release_required = false;
        }
        return H_PATTERN_CALIBRATION_NO_CAPTURE;
    }
    if (!service->advance_pending && !service->advance_input_active) {
        return H_PATTERN_CALIBRATION_NO_CAPTURE;
    }

    service->advance_pending = false;
    HPatternCalibrationResult result =
        capture_position(&service->session, lateral_position, longitudinal_position, settings);
    if (result == H_PATTERN_CALIBRATION_COMPLETED) {
        service->active = false;
    } else if (result == H_PATTERN_CALIBRATION_CAPTURED) {
        service->release_required = true;
    }
    return result;
}
