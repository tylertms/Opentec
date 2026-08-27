#include "force_feedback/safety.h"

#include <stdbool.h>
#include <stdint.h>

void force_safety_init(ForceSafety *safety) {
    safety->blockers = FORCE_SAFETY_MOTOR_UNAVAILABLE | FORCE_SAFETY_WHEEL_UNAVAILABLE |
                       FORCE_SAFETY_PROTOCOL_UNAVAILABLE | FORCE_SAFETY_COMMAND_EXPIRED |
                       FORCE_SAFETY_TEMPERATURE_UNSAFE | FORCE_SAFETY_OUTPUT_INHIBITED;
    safety->armed = false;
}

void force_safety_update(ForceSafety *safety, const ForceSafetyInputs *inputs) {
    uint16_t blockers = 0;
    if (!inputs->motor_ready) {
        blockers |= FORCE_SAFETY_MOTOR_UNAVAILABLE;
    }
    if (!inputs->wheel_ready) {
        blockers |= FORCE_SAFETY_WHEEL_UNAVAILABLE;
    }
    if (!inputs->protocol_ready) {
        blockers |= FORCE_SAFETY_PROTOCOL_UNAVAILABLE;
    }
    if (!inputs->command_fresh) {
        blockers |= FORCE_SAFETY_COMMAND_EXPIRED;
    }
    if (!inputs->temperature_safe) {
        blockers |= FORCE_SAFETY_TEMPERATURE_UNSAFE;
    }
    if (!inputs->output_allowed) {
        blockers |= FORCE_SAFETY_OUTPUT_INHIBITED;
    }
    if (blockers != 0) {
        safety->armed = false;
    }
    safety->blockers = blockers;
}

bool force_safety_arm(ForceSafety *safety) {
    safety->armed = safety->blockers == 0;
    return safety->armed;
}

void force_safety_disarm(ForceSafety *safety) { safety->armed = false; }

bool force_safety_permitted(const ForceSafety *safety) {
    return safety->armed && safety->blockers == 0;
}

uint16_t force_safety_blockers(const ForceSafety *safety) { return safety->blockers; }
