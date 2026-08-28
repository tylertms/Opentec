#ifndef OPENTEC_BASE_PEDAL_CALIBRATION_COMMAND_H
#define OPENTEC_BASE_PEDAL_CALIBRATION_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/protocol.h"
#include "usb/operating_mode_command.h"

typedef enum {
    PEDAL_CALIBRATION_COMMAND_UP,
    PEDAL_CALIBRATION_COMMAND_DOWN,
    PEDAL_CALIBRATION_COMMAND_ENABLE,
    PEDAL_CALIBRATION_COMMAND_DISABLE,
    PEDAL_CALIBRATION_COMMAND_INPUT,
} PedalCalibrationCommandKind;

typedef enum {
    PEDAL_AUXILIARY_CALIBRATION_NONE,
    PEDAL_AUXILIARY_CALIBRATION_MINIMUM,
    PEDAL_AUXILIARY_CALIBRATION_MAXIMUM,
    PEDAL_AUXILIARY_CALIBRATION_RESET,
} PedalAuxiliaryCalibrationAction;

typedef struct {
    PedalCalibrationCommandKind kind;
    uint8_t input[PEDAL_INPUT_AXIS_COUNT];
} PedalCalibrationCommand;

typedef struct {
    PedalV3Control pedal_control;
    PedalAuxiliaryCalibrationAction auxiliary_action;
    uint8_t pedal_input[PEDAL_INPUT_AXIS_COUNT];
    bool pedal_input_pending;
} PedalCalibrationActions;

bool pedal_calibration_command_decode(const UsbOperatingModeCommand *source,
                                      PedalCalibrationCommand *command);
PedalCalibrationActions pedal_calibration_command_route(const PedalCalibrationCommand *command,
                                                        bool pedal_calibration_active,
                                                        bool auxiliary_active);

#endif
