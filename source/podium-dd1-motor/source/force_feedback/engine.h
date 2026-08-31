#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_ENGINE_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/effect.h"
#include "force_feedback/soft_stop.h"

#define MOTOR_FORCE_FEEDBACK_EFFECT_COUNT 20U

typedef enum {
    MOTOR_FORCE_FEEDBACK_EFFECT_NONE = 0,
    MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT = 8,
    MOTOR_FORCE_FEEDBACK_EFFECT_WINDOW = 11,
    MOTOR_FORCE_FEEDBACK_EFFECT_DIRECTIONAL = 12,
} MotorForceFeedbackEffectType;

typedef struct {
    MotorForceFeedbackEffectType type;
    bool active;
    union {
        MotorConstantEffect constant;
        MotorWindowEffect window;
        MotorDirectionalEffect directional;
    } data;
} MotorForceFeedbackEffect;

typedef struct {
    MotorForceFeedbackSettings settings;
    MotorForceFeedbackEffect effects[MOTOR_FORCE_FEEDBACK_EFFECT_COUNT];
    MotorDirectionalEffect window_compensation;
    MotorForceFeedbackFilter filter;
    MotorForceFeedbackSoftStop soft_stop;
    uint16_t soft_stop_transition_range;
    uint8_t ramp_percent;
} MotorForceFeedbackEngine;

typedef struct {
    MotorForceFeedbackOutput primary;
    int32_t secondary;
} MotorForceFeedbackMix;

void motor_force_feedback_engine_initialize(MotorForceFeedbackEngine *engine);
bool motor_force_feedback_constant_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                             const uint8_t payload[5]);
bool motor_force_feedback_window_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                           const uint8_t payload[5]);
bool motor_force_feedback_directional_configure(MotorForceFeedbackEngine *engine, uint8_t slot,
                                                const uint8_t payload[5]);
bool motor_force_feedback_effect_enable(MotorForceFeedbackEngine *engine, uint8_t slot);
bool motor_force_feedback_effect_disable(MotorForceFeedbackEngine *engine, uint8_t slot);
MotorForceFeedbackMix motor_force_feedback_mix(MotorForceFeedbackEngine *engine, uint32_t now,
                                               int32_t centered_position, int32_t soft_stop_center,
                                               int32_t soft_stop_position, int32_t velocity,
                                               bool soft_stop_disabled);

#endif
