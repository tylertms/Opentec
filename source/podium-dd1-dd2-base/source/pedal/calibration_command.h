#ifndef OPENTEC_BASE_PEDAL_CALIBRATION_COMMAND_H
#define OPENTEC_BASE_PEDAL_CALIBRATION_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/protocol.h"
#include "usb/operating_mode_command.h"

/**
 * @brief Identifies a decoded pedal calibration command.
 */
typedef enum {
    PEDAL_CALIBRATION_COMMAND_UP,      /**< Move pedal calibration upward. */
    PEDAL_CALIBRATION_COMMAND_DOWN,    /**< Move pedal calibration downward. */
    PEDAL_CALIBRATION_COMMAND_ENABLE,  /**< Enable pedal calibration. */
    PEDAL_CALIBRATION_COMMAND_DISABLE, /**< Disable pedal calibration. */
    PEDAL_CALIBRATION_COMMAND_INPUT,   /**< Send calibration input values. */
} PedalCalibrationCommandKind;

/**
 * @brief Identifies a local auxiliary calibration action.
 */
typedef enum {
    PEDAL_AUXILIARY_CALIBRATION_NONE,    /**< No auxiliary calibration action. */
    PEDAL_AUXILIARY_CALIBRATION_MINIMUM, /**< Capture the auxiliary minimum. */
    PEDAL_AUXILIARY_CALIBRATION_MAXIMUM, /**< Capture the auxiliary maximum. */
    PEDAL_AUXILIARY_CALIBRATION_RESET,   /**< Reset auxiliary calibration. */
} PedalAuxiliaryCalibrationAction;

/**
 * @brief Stores a decoded pedal calibration command.
 */
typedef struct {
    PedalCalibrationCommandKind kind;      /**< Decoded command kind. */
    uint8_t input[PEDAL_INPUT_AXIS_COUNT]; /**< Three calibration input values. */
} PedalCalibrationCommand;

/**
 * @brief Stores routed pedal and auxiliary calibration actions.
 */
typedef struct {
    PedalV3Control pedal_control;                     /**< V3 pedal control bits to queue. */
    PedalAuxiliaryCalibrationAction auxiliary_action; /**< Local auxiliary action to apply. */
    uint8_t pedal_input[PEDAL_INPUT_AXIS_COUNT];      /**< Three pedal input values to queue. */
    bool pedal_input_pending; /**< True when pedal_input contains a queued command. */
} PedalCalibrationActions;

/**
 * @brief Decodes a pedal calibration command from an operating-mode command.
 *
 * Recognizes the five supported calibration selectors and copies their input bytes.
 *
 * @param[in] source Decoded operating-mode command.
 * @param[out] command Destination for the decoded calibration command.
 * @return True when source identifies a supported calibration command.
 */
bool pedal_calibration_command_decode(const UsbOperatingModeCommand *source,
                                      PedalCalibrationCommand *command);

/**
 * @brief Routes a decoded calibration command to active pedal and auxiliary sources.
 *
 * Produces independent actions for the attached pedals and local auxiliary input according to the
 * latest source sample and command values.
 *
 * @param[in] command Decoded calibration command.
 * @param[in] pedal_calibration_active True when attached pedals accept calibration controls.
 * @param[in] auxiliary_axis_sample Latest raw local auxiliary-axis ADC sample. Source presence is
 * probed from this sample for each command invocation.
 * @return Routed pedal and auxiliary actions.
 */
PedalCalibrationActions pedal_calibration_command_route(const PedalCalibrationCommand *command,
                                                        bool pedal_calibration_active,
                                                        uint16_t auxiliary_axis_sample);

#endif
