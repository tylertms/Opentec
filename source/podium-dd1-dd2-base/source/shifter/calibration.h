#ifndef OPENTEC_BASE_SHIFTER_CALIBRATION_H
#define OPENTEC_BASE_SHIFTER_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "shifter/h_pattern.h"
#include "usb/operating_mode_command.h"

typedef enum {
    H_PATTERN_CALIBRATION_COMMAND_START,
    H_PATTERN_CALIBRATION_COMMAND_ADVANCE,
} HPatternCalibrationCommand;

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

typedef enum {
    H_PATTERN_CALIBRATION_NEUTRAL,
    H_PATTERN_CALIBRATION_REVERSE,
    H_PATTERN_CALIBRATION_FIRST,
    H_PATTERN_CALIBRATION_SECOND,
    H_PATTERN_CALIBRATION_THIRD,
    H_PATTERN_CALIBRATION_FOURTH,
    H_PATTERN_CALIBRATION_FIFTH,
    H_PATTERN_CALIBRATION_SIXTH,
    H_PATTERN_CALIBRATION_SEVENTH,
    H_PATTERN_CALIBRATION_COMPLETE,
} HPatternCalibrationPosition;

typedef enum {
    H_PATTERN_CALIBRATION_NO_CAPTURE,
    H_PATTERN_CALIBRATION_CAPTURED,
    H_PATTERN_CALIBRATION_COMPLETED,
} HPatternCalibrationResult;

typedef enum {
    H_PATTERN_CALIBRATION_PROMPT_NONE,
    H_PATTERN_CALIBRATION_PROMPT_WAITING,
    H_PATTERN_CALIBRATION_PROMPT_SHIFTER,
    H_PATTERN_CALIBRATION_PROMPT_CALIBRATION,
    H_PATTERN_CALIBRATION_PROMPT_POSITION,
} HPatternCalibrationPrompt;

typedef struct {
    HPatternCalibrationPosition next_position;
    HPatternCalibrationSamples samples;
} HPatternCalibrationSession;

typedef struct {
    HPatternCalibrationSession session;
    uint32_t started_ms;
    uint8_t wheel_mode;
    bool active;
    bool advance_pending;
    bool advance_input_active;
    bool release_required;
} HPatternCalibrationService;

bool h_pattern_calibration_command_decode(const UsbOperatingModeCommand *source,
                                          HPatternCalibrationCommand *command);
HPatternCalibration h_pattern_calibration_build(const HPatternCalibrationSamples *samples);
void h_pattern_calibration_service_request(HPatternCalibrationService *service,
                                           HPatternCalibrationCommand command, uint8_t wheel_mode,
                                           uint32_t now_ms);
bool h_pattern_calibration_service_start_if_required(HPatternCalibrationService *service,
                                                     bool start_allowed, bool calibrated,
                                                     uint8_t wheel_mode, uint32_t now_ms);
void h_pattern_calibration_service_set_advance_input(HPatternCalibrationService *service,
                                                     bool active);
HPatternCalibrationResult h_pattern_calibration_service_capture(HPatternCalibrationService *service,
                                                                uint32_t now_ms,
                                                                uint16_t lateral_position,
                                                                uint16_t longitudinal_position,
                                                                HPatternSettings *settings);
HPatternCalibrationPrompt
h_pattern_calibration_service_prompt(const HPatternCalibrationService *service, uint32_t now_ms);

#endif
