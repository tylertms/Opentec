#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SAFETY_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FORCE_SAFETY_MOTOR_UNAVAILABLE = 1 << 0,
    FORCE_SAFETY_WHEEL_UNAVAILABLE = 1 << 1,
    FORCE_SAFETY_PROTOCOL_UNAVAILABLE = 1 << 2,
    FORCE_SAFETY_COMMAND_EXPIRED = 1 << 3,
    FORCE_SAFETY_TEMPERATURE_UNSAFE = 1 << 4,
    FORCE_SAFETY_OUTPUT_INHIBITED = 1 << 5,
} ForceSafetyBlocker;

typedef struct {
    bool motor_ready;
    bool wheel_ready;
    bool protocol_ready;
    bool command_fresh;
    bool temperature_safe;
    bool output_allowed;
} ForceSafetyInputs;

typedef struct {
    uint16_t blockers;
    bool armed;
} ForceSafety;

void force_safety_init(ForceSafety *safety);
void force_safety_update(ForceSafety *safety, const ForceSafetyInputs *inputs);
bool force_safety_arm(ForceSafety *safety);
void force_safety_disarm(ForceSafety *safety);
bool force_safety_permitted(const ForceSafety *safety);
uint16_t force_safety_blockers(const ForceSafety *safety);

#endif
