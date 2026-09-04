#include "shifter/calibration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief H-pattern calibration protocol and timing constants.
 */
enum {
    H_PATTERN_CALIBRATION_OPCODE = 0x19, /**< Operating-mode opcode for H-pattern calibration. */
    H_PATTERN_CALIBRATION_START_SELECTOR = 1,   /**< Selector that starts calibration. */
    H_PATTERN_CALIBRATION_ADVANCE_SELECTOR = 2, /**< Selector that advances calibration. */
    H_PATTERN_EXTENDED_WHEEL_MODE = 0x1c, /**< Wheel mode with extended calibration presentation. */
    H_PATTERN_LEGACY_SHIFTER_PROMPT_MS = 2000, /**< Legacy shifter-label presentation duration. */
    H_PATTERN_LEGACY_CALIBRATION_PROMPT_MS = 4000, /**< Legacy calibration-label deadline. */
    H_PATTERN_EXTENDED_READY_PROMPT_MS = 1000, /**< Extended ready-label presentation duration. */
    H_PATTERN_EXTENDED_ENTRY_DELAY_MS = 5000,  /**< Extended-mode entry delay. */
    H_PATTERN_EXTENDED_COMPLETION_MS = 1000,   /**< Extended-mode completion hold duration. */
    H_PATTERN_CALIBRATION_REPORT_INTERVAL_MS = 2000, /**< Unchanged-stage report interval. */
    SEVENTH_BOUNDARY_MINIMUM_SPAN = 5, /**< Maximum fifth-to-seventh span for fallback boundary. */
    SEVENTH_BOUNDARY_FALLBACK_OFFSET = 20, /**< Offset below fifth gear for fallback boundary. */
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
        uint16_t retained_boundary = settings->calibration.retained_boundary;
        settings->calibration = h_pattern_calibration_build(&session->samples);
        settings->calibration.retained_boundary = retained_boundary;
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
 * next available H-pattern analog sample. A new session records its wheel mode and start time for
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
 * @brief Updates the protocol completion-button input level.
 *
 * Retains the protocol-only button state used to release a completed calibration session.
 *
 * @param[in,out] service Calibration lifecycle state.
 * @param[in] active True while the protocol completion button is active.
 */
void h_pattern_calibration_service_set_completion_input(HPatternCalibrationService *service,
                                                        bool active) {
    if (service != NULL) {
        service->completion_input_active = active;
    }
}

/**
 * @brief Cancels the current calibration session.
 *
 * Retains the caller's persisted settings while clearing the in-progress session and every
 * transient input or timing latch.
 *
 * @param[in,out] service Calibration lifecycle state to cancel.
 */
void h_pattern_calibration_service_cancel(HPatternCalibrationService *service) {
    if (service == NULL) {
        return;
    }
    service->session = (HPatternCalibrationSession){0};
    service->started_ms = 0;
    service->finish_deadline_ms = 0;
    service->wheel_mode = 0;
    service->active = false;
    service->advance_pending = false;
    service->advance_input_active = false;
    service->completion_input_active = false;
    service->release_required = false;
}

/**
 * @brief Maps H-pattern lifecycle state to the official wheel-report stage.
 *
 * Entry presentation reports the ready, start, and wait stages before capture. Each capture and
 * required release uses the corresponding official pair, and completed ownership reports stage 23.
 *
 * @param[in] service Calibration lifecycle state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Current official calibration stage.
 */
HPatternCalibrationStage
h_pattern_calibration_service_stage(const HPatternCalibrationService *service, uint32_t now_ms) {
    if (service == NULL || !service->active) {
        return H_PATTERN_CALIBRATION_STAGE_DETECT_INPUT;
    }
    if (service->session.next_position == H_PATTERN_CALIBRATION_COMPLETE) {
        return H_PATTERN_CALIBRATION_STAGE_COMPLETE;
    }

    if (service->session.next_position == H_PATTERN_CALIBRATION_NEUTRAL) {
        uint32_t elapsed_ms = now_ms - service->started_ms;
        uint32_t ready_deadline_ms = service->wheel_mode == H_PATTERN_EXTENDED_WHEEL_MODE
                                         ? H_PATTERN_EXTENDED_READY_PROMPT_MS
                                         : H_PATTERN_LEGACY_SHIFTER_PROMPT_MS;
        uint32_t start_deadline_ms = service->wheel_mode == H_PATTERN_EXTENDED_WHEEL_MODE
                                         ? H_PATTERN_EXTENDED_ENTRY_DELAY_MS
                                         : H_PATTERN_LEGACY_CALIBRATION_PROMPT_MS;
        if (elapsed_ms <= ready_deadline_ms) {
            return H_PATTERN_CALIBRATION_STAGE_SHOW_READY;
        }
        if (service->wheel_mode != H_PATTERN_EXTENDED_WHEEL_MODE &&
            elapsed_ms <= start_deadline_ms) {
            return H_PATTERN_CALIBRATION_STAGE_SHOW_START;
        }
        if (!service->advance_pending && !service->advance_input_active) {
            return H_PATTERN_CALIBRATION_STAGE_WAIT_START;
        }
    }

    uint16_t stage = (uint16_t)H_PATTERN_CALIBRATION_STAGE_CAPTURE_NEUTRAL +
                     (uint16_t)service->session.next_position * 2u;
    if (service->release_required) {
        stage++;
    }
    return (HPatternCalibrationStage)stage;
}

/**
 * @brief Publishes a changed or due H-pattern stage through type 0x16.
 *
 * State changes are queued immediately. Once capture starts, an attached wheel receives the
 * unchanged state again only while connected and at the inclusive two-second deadline. A repeated
 * report starts a fresh interval from the current scheduler time; state changes and disconnects do
 * not move the retained deadline.
 *
 * @param[in,out] service Calibration lifecycle and report cadence state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] adapter_connected True when the attached-wheel link is connected.
 * @param[out] report Three-byte type-0x16 payload destination.
 * @return True when a report should be queued; otherwise false.
 */
bool h_pattern_calibration_service_take_report(HPatternCalibrationService *service, uint32_t now_ms,
                                               bool adapter_connected, uint8_t report[3]) {
    if (service == NULL || report == NULL) {
        return false;
    }

    HPatternCalibrationStage stage = h_pattern_calibration_service_stage(service, now_ms);
    service->report_state = stage;
    bool changed = stage != service->reported_state;
    bool repeated = !changed && stage > H_PATTERN_CALIBRATION_STAGE_SHOW_READY &&
                    adapter_connected && now_ms >= service->report_deadline_ms;
    if (!changed && !repeated) {
        return false;
    }

    report[0] = 0;
    report[1] = (uint8_t)stage;
    report[2] = (uint8_t)((uint16_t)stage >> 8);
    service->reported_state = stage;
    if (repeated) {
        service->report_deadline_ms = now_ms + H_PATTERN_CALIBRATION_REPORT_INTERVAL_MS;
    }
    return true;
}

/**
 * @brief Captures a queued H-pattern calibration position.
 *
 * Ignores samples until the entry presentation completes and either the attached-wheel input or a
 * host advance is active. After each capture, waits for the attached-wheel input to be released
 * before accepting the next position. A successful seventh-gear capture enables the new settings
 * and retains calibration ownership while an advance input remains active. Extended mode also
 * retains ownership through a strict one-second completion deadline.
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
        if (!service->completion_input_active && completion_elapsed) {
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
