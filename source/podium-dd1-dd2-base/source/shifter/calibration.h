#ifndef OPENTEC_BASE_SHIFTER_CALIBRATION_H
#define OPENTEC_BASE_SHIFTER_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "shifter/h_pattern.h"
#include "usb/operating_mode_command.h"

/**
 * @brief Identifies an H-pattern calibration command.
 *
 * Commands either start a new capture sequence or advance it to the next position.
 */
typedef enum {
    H_PATTERN_CALIBRATION_COMMAND_START,   /**< Start a new H-pattern calibration sequence. */
    H_PATTERN_CALIBRATION_COMMAND_ADVANCE, /**< Request capture of the next H-pattern calibration
                                              position. */
} HPatternCalibrationCommand;

/**
 * @brief Stores the axis samples captured during H-pattern calibration.
 *
 * The samples provide the neutral, reverse, and forward gear references used to build thresholds.
 */
typedef struct {
    uint16_t neutral_longitudinal; /**< Neutral longitudinal-axis sample. */
    uint16_t reverse_lateral;      /**< Reverse lateral-axis sample. */
    uint16_t reverse_longitudinal; /**< Reverse longitudinal-axis sample. */
    uint16_t first_lateral;        /**< First-gear lateral-axis sample. */
    uint16_t second_lateral;       /**< Second-gear lateral-axis sample. */
    uint16_t second_longitudinal;  /**< Second-gear longitudinal-axis sample. */
    uint16_t third_lateral;        /**< Third-gear lateral-axis sample. */
    uint16_t fourth_lateral;       /**< Fourth-gear lateral-axis sample. */
    uint16_t fifth_lateral;        /**< Fifth-gear lateral-axis sample. */
    uint16_t sixth_lateral;        /**< Sixth-gear lateral-axis sample. */
    uint16_t seventh_lateral;      /**< Seventh-gear lateral-axis sample. */
} HPatternCalibrationSamples;

/**
 * @brief Identifies the next H-pattern calibration position.
 *
 * Positions are captured in this order from neutral through seventh gear, followed by complete.
 */
typedef enum {
    H_PATTERN_CALIBRATION_NEUTRAL,  /**< Capture neutral. */
    H_PATTERN_CALIBRATION_REVERSE,  /**< Capture reverse. */
    H_PATTERN_CALIBRATION_FIRST,    /**< Capture first gear. */
    H_PATTERN_CALIBRATION_SECOND,   /**< Capture second gear. */
    H_PATTERN_CALIBRATION_THIRD,    /**< Capture third gear. */
    H_PATTERN_CALIBRATION_FOURTH,   /**< Capture fourth gear. */
    H_PATTERN_CALIBRATION_FIFTH,    /**< Capture fifth gear. */
    H_PATTERN_CALIBRATION_SIXTH,    /**< Capture sixth gear. */
    H_PATTERN_CALIBRATION_SEVENTH,  /**< Capture seventh gear. */
    H_PATTERN_CALIBRATION_COMPLETE, /**< All H-pattern calibration positions have been captured. */
} HPatternCalibrationPosition;

/**
 * @brief Describes the result of an H-pattern calibration capture attempt.
 */
typedef enum {
    H_PATTERN_CALIBRATION_NO_CAPTURE, /**< No position was captured. */
    H_PATTERN_CALIBRATION_CAPTURED,   /**< An intermediate position was captured. */
    H_PATTERN_CALIBRATION_COMPLETED,  /**< The final position was captured and settings were
                                         completed. */
} HPatternCalibrationResult;

/**
 * @brief Identifies the H-pattern calibration display prompt.
 */
typedef enum {
    H_PATTERN_CALIBRATION_PROMPT_NONE,    /**< No calibration prompt is active. */
    H_PATTERN_CALIBRATION_PROMPT_WAITING, /**< Calibration is waiting through its entry delay. */
    H_PATTERN_CALIBRATION_PROMPT_SHIFTER, /**< Display the shifter calibration label. */
    H_PATTERN_CALIBRATION_PROMPT_CALIBRATION, /**< Display the calibration label. */
    H_PATTERN_CALIBRATION_PROMPT_POSITION, /**< Display the position currently awaiting capture. */
} HPatternCalibrationPrompt;

/**
 * @brief Retains one H-pattern calibration capture session.
 */
typedef struct {
    HPatternCalibrationPosition next_position; /**< Position to capture next. */
    HPatternCalibrationSamples samples;        /**< Axis samples captured so far. */
} HPatternCalibrationSession;

/**
 * @brief Retains H-pattern calibration lifecycle and input gating state.
 *
 * The service separates entry presentation, capture requests, release gating, and completion
 * ownership from the persistent H-pattern settings.
 */
typedef struct {
    HPatternCalibrationSession session; /**< Current capture session and collected samples. */
    uint32_t started_ms;                /**< Monotonic time at which the current session started. */
    uint32_t finish_deadline_ms;        /**< Completion deadline for extended-wheel presentation. */
    uint8_t wheel_mode;        /**< Attached-wheel mode used to select presentation timing. */
    bool active;               /**< True while a calibration session owns the input and display. */
    bool advance_pending;      /**< True when a host advance request awaits capture. */
    bool advance_input_active; /**< Current physical calibration-advance input level. */
    bool release_required;     /**< True until the advance input is released after a capture. */
} HPatternCalibrationService;

/**
 * @brief Decodes an H-pattern calibration operating-mode command.
 *
 * Recognizes the start and advance selectors for the H-pattern calibration protocol.
 *
 * @param[in] source Decoded operating-mode command.
 * @param[out] command Destination for the decoded calibration command.
 * @return True when source identifies a supported calibration command.
 */
bool h_pattern_calibration_command_decode(const UsbOperatingModeCommand *source,
                                          HPatternCalibrationCommand *command);

/**
 * @brief Builds H-pattern thresholds from captured axis samples.
 *
 * Calculates adjacent-gear lateral boundaries and neutral-to-row longitudinal thresholds.
 *
 * @param[in] samples Captured neutral, reverse, and forward-gear samples.
 * @return Calibration thresholds for H-pattern classification.
 */
HPatternCalibration h_pattern_calibration_build(const HPatternCalibrationSamples *samples);

/**
 * @brief Applies an H-pattern calibration lifecycle command.
 *
 * Starts a fresh session for start commands and queues a capture request for advance commands.
 *
 * @param[in,out] service Calibration lifecycle state to update.
 * @param[in] command Start or advance command to apply.
 * @param[in] wheel_mode Attached-wheel mode used for presentation timing.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void h_pattern_calibration_service_request(HPatternCalibrationService *service,
                                           HPatternCalibrationCommand command, uint8_t wheel_mode,
                                           uint32_t now_ms);

/**
 * @brief Starts calibration when an uncalibrated H-pattern input is ready.
 *
 * Leaves active sessions and already calibrated settings unchanged.
 *
 * @param[in,out] service Calibration lifecycle state to update.
 * @param[in] start_allowed True when H-pattern input and display entry are available.
 * @param[in] calibrated True when usable H-pattern settings already exist.
 * @param[in] wheel_mode Attached-wheel mode used for presentation timing.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when a new calibration session was started.
 */
bool h_pattern_calibration_service_start_if_required(HPatternCalibrationService *service,
                                                     bool start_allowed, bool calibrated,
                                                     uint8_t wheel_mode, uint32_t now_ms);

/**
 * @brief Updates the physical calibration-advance input level.
 *
 * The service uses a release between captures to prevent one held input from advancing repeatedly.
 *
 * @param[in,out] service Calibration lifecycle state to update.
 * @param[in] active True while the calibration-advance input is active.
 */
void h_pattern_calibration_service_set_advance_input(HPatternCalibrationService *service,
                                                     bool active);

/**
 * @brief Captures the next H-pattern calibration position when permitted.
 *
 * Applies entry timing, pending-advance requests, release gating, and completion handling before
 * storing the supplied axis samples.
 *
 * @param[in,out] service Calibration lifecycle state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] lateral_position Current lateral-axis sample.
 * @param[in] longitudinal_position Current longitudinal-axis sample.
 * @param[in,out] settings H-pattern settings to complete on the final capture.
 * @return No capture, an intermediate capture, or completed calibration.
 */
HPatternCalibrationResult h_pattern_calibration_service_capture(HPatternCalibrationService *service,
                                                                uint32_t now_ms,
                                                                uint16_t lateral_position,
                                                                uint16_t longitudinal_position,
                                                                HPatternSettings *settings);
/**
 * @brief Selects the current H-pattern calibration display prompt.
 *
 * Applies wheel-mode entry timing and returns the position prompt after the entry presentation.
 *
 * @param[in] service Calibration lifecycle state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Current calibration display prompt.
 */
HPatternCalibrationPrompt
h_pattern_calibration_service_prompt(const HPatternCalibrationService *service, uint32_t now_ms);

#endif
