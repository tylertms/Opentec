#include "shifter/calibration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    H_PATTERN_CALIBRATION_OPCODE = 0x19,
    H_PATTERN_CALIBRATION_START_SELECTOR = 1,
    H_PATTERN_CALIBRATION_ADVANCE_SELECTOR = 2,
    H_PATTERN_EXTENDED_WHEEL_MODE = 0x1c,
    H_PATTERN_LEGACY_SHIFTER_PROMPT_MS = 2000,
    H_PATTERN_LEGACY_CALIBRATION_PROMPT_MS = 4000,
    H_PATTERN_EXTENDED_ENTRY_DELAY_MS = 5000,
    H_PATTERN_EXTENDED_COMPLETION_MS = 1000,
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
 * next available H-pattern analog sample. A new session retains its wheel mode and start time for
 * entry presentation and capture gating.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] command Requested start or advance action.
 * @param[in] wheel_mode Active attached-wheel mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void h_pattern_calibration_service_request(HPatternCalibrationService *service,
                                           HPatternCalibrationCommand command, uint8_t wheel_mode,
                                           uint32_t now_ms) {
    if (service == NULL) {
        return;
    }

    if (command == H_PATTERN_CALIBRATION_COMMAND_START) {
        *service = (HPatternCalibrationService){
            .started_ms = now_ms,
            .wheel_mode = wheel_mode,
            .active = true,
        };
    } else if (command == H_PATTERN_CALIBRATION_COMMAND_ADVANCE) {
        service->advance_pending = true;
    }
}

/**
 * @brief Starts calibration for an uninitialized H-pattern input.
 *
 * Opens a fresh capture session when the local entry path is ready without saved calibration. An
 * active session or calibrated input remains unchanged.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] start_allowed True when an H-pattern input and attached-wheel display are available.
 * @param[in] calibrated True when usable H-pattern thresholds are already available.
 * @param[in] wheel_mode Active attached-wheel mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when a new calibration session was started.
 */
bool h_pattern_calibration_service_start_if_required(HPatternCalibrationService *service,
                                                     bool start_allowed, bool calibrated,
                                                     uint8_t wheel_mode, uint32_t now_ms) {
    if (service == NULL || service->active || !start_allowed || calibrated) {
        return false;
    }

    h_pattern_calibration_service_request(service, H_PATTERN_CALIBRATION_COMMAND_START, wheel_mode,
                                          now_ms);
    return true;
}

/**
 * @brief Selects the current H-pattern calibration prompt.
 *
 * Legacy wheel modes show the shifter and calibration labels for two seconds each before the first
 * position. Extended mode reserves five seconds for its separate presentation path. Captures are
 * accepted only after the corresponding entry interval expires. Completion returns no prompt
 * while calibration ownership waits for release.
 *
 * @param[in] service Calibration lifecycle state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Inactive, waiting, shifter, calibration, or position prompt state.
 */
HPatternCalibrationPrompt
h_pattern_calibration_service_prompt(const HPatternCalibrationService *service, uint32_t now_ms) {
    if (service == NULL || !service->active) {
        return H_PATTERN_CALIBRATION_PROMPT_NONE;
    }
    if (service->session.next_position == H_PATTERN_CALIBRATION_COMPLETE) {
        return H_PATTERN_CALIBRATION_PROMPT_NONE;
    }

    uint32_t elapsed_ms = now_ms - service->started_ms;
    if (service->wheel_mode == H_PATTERN_EXTENDED_WHEEL_MODE) {
        return elapsed_ms <= H_PATTERN_EXTENDED_ENTRY_DELAY_MS
                   ? H_PATTERN_CALIBRATION_PROMPT_WAITING
                   : H_PATTERN_CALIBRATION_PROMPT_POSITION;
    }
    if (elapsed_ms <= H_PATTERN_LEGACY_SHIFTER_PROMPT_MS) {
        return H_PATTERN_CALIBRATION_PROMPT_SHIFTER;
    }
    return elapsed_ms <= H_PATTERN_LEGACY_CALIBRATION_PROMPT_MS
               ? H_PATTERN_CALIBRATION_PROMPT_CALIBRATION
               : H_PATTERN_CALIBRATION_PROMPT_POSITION;
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
 * Ignores samples until the entry presentation completes and either the attached-wheel input or a
 * host advance is active. After each capture, waits for the attached-wheel input to be released
 * before accepting the next position. A successful seventh-gear capture enables the new settings,
 * then retains calibration ownership until input release. Extended mode also retains ownership
 * through a strict one-second completion deadline.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] lateral_position Current lateral axis sample.
 * @param[in] longitudinal_position Current longitudinal axis sample.
 * @param[in,out] settings Destination for the completed calibration.
 * @return No capture, an intermediate capture, or completed calibration.
 */
HPatternCalibrationResult h_pattern_calibration_service_capture(HPatternCalibrationService *service,
                                                                uint32_t now_ms,
                                                                uint16_t lateral_position,
                                                                uint16_t longitudinal_position,
                                                                HPatternSettings *settings) {
    if (service == NULL || settings == NULL) {
        return H_PATTERN_CALIBRATION_NO_CAPTURE;
    }
    if (service->active && service->session.next_position == H_PATTERN_CALIBRATION_COMPLETE) {
        bool completion_elapsed = service->wheel_mode != H_PATTERN_EXTENDED_WHEEL_MODE ||
                                  (int32_t)(now_ms - service->finish_deadline_ms) > 0;
        if (!service->advance_input_active && completion_elapsed) {
            service->active = false;
        }
        return H_PATTERN_CALIBRATION_NO_CAPTURE;
    }
    if (h_pattern_calibration_service_prompt(service, now_ms) !=
        H_PATTERN_CALIBRATION_PROMPT_POSITION) {
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
        service->finish_deadline_ms = now_ms + H_PATTERN_EXTENDED_COMPLETION_MS;
    } else if (result == H_PATTERN_CALIBRATION_CAPTURED) {
        service->release_required = true;
    }
    return result;
}
