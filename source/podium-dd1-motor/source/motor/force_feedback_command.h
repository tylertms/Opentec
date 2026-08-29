#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_COMMAND_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/force_feedback_engine.h"

bool motor_force_feedback_command_apply(MotorForceFeedbackEngine *engine, const uint8_t command[7]);

#endif
