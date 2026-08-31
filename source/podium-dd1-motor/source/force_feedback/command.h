#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_COMMAND_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/engine.h"

/**
 * @brief Applies one seven-byte motor-link force-feedback command.
 *
 * Decodes the slot and opcode, then configures or changes the activation of the addressed effect.
 * Unknown opcodes are accepted without changing the engine.
 *
 * @param[in,out] engine Force-feedback engine to update.
 * @param[in] command Seven-byte command containing slot, opcode, effect kind, and payload.
 * @return True when the command is accepted or unknown; false when a known command names an
 *         unsupported effect kind or slot.
 */
bool motor_force_feedback_command_apply(MotorForceFeedbackEngine *engine, const uint8_t command[7]);

#endif
