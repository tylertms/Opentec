#include "pedal/calibration_command.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pedal/protocol.h"
#include "usb/operating_mode_command.h"

enum {
    PEDAL_CALIBRATION_OPCODE = 1,
    PEDAL_CALIBRATION_UP_SELECTOR = 0x11,
    PEDAL_CALIBRATION_DOWN_SELECTOR = 0x12,
    PEDAL_CALIBRATION_ENABLE_SELECTOR = 0x13,
    PEDAL_CALIBRATION_DISABLE_SELECTOR = 0x14,
    PEDAL_CALIBRATION_INPUT_SELECTOR = 0x15,
    PEDAL_AUXILIARY_INPUT_GROUP = 1,
    PEDAL_AUXILIARY_INPUT_CHANNEL = 3,
};

/**
 * @brief Decodes a pedal-calibration command from the operating-mode envelope.
 *
 * Accepts opcode one and selectors 0x11 through 0x15. Input commands retain all three payload
 * bytes for later routing to the attached pedals or local auxiliary input.
 *
 * @param[in] source Decoded F8 09 operating-mode command.
 * @param[out] command Pedal-calibration command and input values.
 * @return True when the opcode and selector identify a supported calibration command.
 */
bool pedal_calibration_command_decode(const UsbOperatingModeCommand *source,
                                      PedalCalibrationCommand *command) {
    if (source == NULL || command == NULL || source->opcode != PEDAL_CALIBRATION_OPCODE) {
        return false;
    }

    PedalCalibrationCommandKind kind;
    switch (source->parameters[0]) {
    case PEDAL_CALIBRATION_UP_SELECTOR:
        kind = PEDAL_CALIBRATION_COMMAND_UP;
        break;
    case PEDAL_CALIBRATION_DOWN_SELECTOR:
        kind = PEDAL_CALIBRATION_COMMAND_DOWN;
        break;
    case PEDAL_CALIBRATION_ENABLE_SELECTOR:
        kind = PEDAL_CALIBRATION_COMMAND_ENABLE;
        break;
    case PEDAL_CALIBRATION_DISABLE_SELECTOR:
        kind = PEDAL_CALIBRATION_COMMAND_DISABLE;
        break;
    case PEDAL_CALIBRATION_INPUT_SELECTOR:
        kind = PEDAL_CALIBRATION_COMMAND_INPUT;
        break;
    default:
        return false;
    }

    *command = (PedalCalibrationCommand){
        .kind = kind,
        .input = {source->parameters[1], source->parameters[2], source->parameters[3]},
    };
    return true;
}

/**
 * @brief Routes one calibration command to attached-pedal and local auxiliary actions.
 *
 * Pedal controls are emitted only while a compatible pedal calibration is active. A connected
 * local auxiliary input independently receives maximum, minimum, or reset actions. Input selector
 * 0x15 reserves payload 1,3,direction for local endpoint capture and otherwise forwards all three
 * values to the pedal calibration path.
 *
 * @param[in] command Decoded pedal-calibration command.
 * @param[in] pedal_calibration_active True when attached pedals accept calibration commands.
 * @param[in] auxiliary_active True when the local auxiliary analog source is connected.
 * @return Independent attached-pedal and local auxiliary actions.
 */
PedalCalibrationActions pedal_calibration_command_route(const PedalCalibrationCommand *command,
                                                        bool pedal_calibration_active,
                                                        bool auxiliary_active) {
    PedalCalibrationActions actions = {0};
    if (command == NULL) {
        return actions;
    }

    switch (command->kind) {
    case PEDAL_CALIBRATION_COMMAND_UP:
        if (pedal_calibration_active) {
            actions.pedal_control = PEDAL_V3_CONTROL_UP;
        }
        if (auxiliary_active) {
            actions.auxiliary_action = PEDAL_AUXILIARY_CALIBRATION_MAXIMUM;
        }
        break;
    case PEDAL_CALIBRATION_COMMAND_DOWN:
        if (pedal_calibration_active) {
            actions.pedal_control = PEDAL_V3_CONTROL_DOWN;
        }
        if (auxiliary_active) {
            actions.auxiliary_action = PEDAL_AUXILIARY_CALIBRATION_MINIMUM;
        }
        break;
    case PEDAL_CALIBRATION_COMMAND_ENABLE:
        if (pedal_calibration_active) {
            actions.pedal_control = PEDAL_V3_CONTROL_ENABLE;
        }
        if (auxiliary_active) {
            actions.auxiliary_action = PEDAL_AUXILIARY_CALIBRATION_RESET;
        }
        break;
    case PEDAL_CALIBRATION_COMMAND_DISABLE:
        if (pedal_calibration_active) {
            actions.pedal_control = PEDAL_V3_CONTROL_DISABLE;
        }
        if (auxiliary_active) {
            actions.auxiliary_action = PEDAL_AUXILIARY_CALIBRATION_RESET;
        }
        break;
    case PEDAL_CALIBRATION_COMMAND_INPUT:
        if (auxiliary_active && command->input[0] == PEDAL_AUXILIARY_INPUT_GROUP &&
            command->input[1] == PEDAL_AUXILIARY_INPUT_CHANNEL) {
            actions.auxiliary_action = command->input[2] == 0 ? PEDAL_AUXILIARY_CALIBRATION_MINIMUM
                                                              : PEDAL_AUXILIARY_CALIBRATION_MAXIMUM;
        } else if (pedal_calibration_active) {
            for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
                actions.pedal_input[axis] = command->input[axis];
            }
            actions.pedal_input_pending = true;
        }
        break;
    }
    return actions;
}
