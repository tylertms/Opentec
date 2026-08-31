#include "force_feedback/command.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Motor-link force-feedback command opcodes handled by this module. */
enum {
    CONFIGURE_OPCODE = 1,          /**< Configures and enables one effect slot. */
    DISABLE_OPCODE = 3,            /**< Disables one host-controlled effect slot. */
    ENABLE_POSITION_OPCODE = 4,    /**< Enables the built-in position effect slot. */
    DISABLE_POSITION_OPCODE = 5,   /**< Disables the built-in position effect slot. */
};

bool motor_force_feedback_command_apply(MotorForceFeedbackEngine *engine,
                                        const uint8_t command[7]) {
    uint8_t slot = command[0] >> 4U;
    uint8_t opcode = command[0] & 0x0fU;
    if (opcode == CONFIGURE_OPCODE) {
        bool configured;
        if (command[1] == MOTOR_FORCE_FEEDBACK_EFFECT_NONE) {
            configured = true;
        } else if (command[1] == MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT) {
            configured = motor_force_feedback_constant_configure(engine, slot, command + 2U);
        } else if (command[1] == MOTOR_FORCE_FEEDBACK_EFFECT_WINDOW) {
            configured = motor_force_feedback_window_configure(engine, slot, command + 2U);
        } else if (command[1] == MOTOR_FORCE_FEEDBACK_EFFECT_DIRECTIONAL) {
            configured = motor_force_feedback_directional_configure(engine, slot, command + 2U);
        } else {
            return false;
        }
        return configured && motor_force_feedback_effect_enable(engine, slot);
    }
    if (opcode == DISABLE_OPCODE) {
        return motor_force_feedback_effect_disable(engine, slot);
    }
    if (opcode == ENABLE_POSITION_OPCODE) {
        return motor_force_feedback_effect_enable(engine, MOTOR_FORCE_FEEDBACK_POSITION_SLOT);
    }
    if (opcode == DISABLE_POSITION_OPCODE) {
        return motor_force_feedback_effect_disable(engine, MOTOR_FORCE_FEEDBACK_POSITION_SLOT);
    }
    return true;
}
