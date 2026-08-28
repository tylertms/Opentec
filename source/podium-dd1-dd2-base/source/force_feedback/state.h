#ifndef OPENTEC_BASE_FORCE_FEEDBACK_STATE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/command.h"

enum { FORCE_FEEDBACK_STATE_EFFECT_COUNT = FORCE_FEEDBACK_EFFECT_SLOT_COUNT + 1 };

typedef enum {
    FORCE_FEEDBACK_EFFECT_NONE,
    FORCE_FEEDBACK_EFFECT_KIND_1,
    FORCE_FEEDBACK_EFFECT_KIND_2,
    FORCE_FEEDBACK_EFFECT_KIND_3,
} ForceFeedbackEffectKind;

typedef struct {
    int32_t magnitude;
} ForceFeedbackKind1Effect;

typedef struct {
    int32_t positions[2];
    uint8_t axis_modes[2];
    int8_t directions[2];
    uint16_t strength;
} ForceFeedbackKind2Effect;

typedef struct {
    uint8_t mode;
    uint8_t axis_mode;
    int8_t directions[2];
    uint16_t strength;
} ForceFeedbackKind3Effect;

typedef struct {
    ForceFeedbackEffectKind kind;
    bool active;
    ForceFeedbackKind1Effect kind_1;
    ForceFeedbackKind2Effect kind_2;
    ForceFeedbackKind3Effect kind_3;
} ForceFeedbackEffectState;

typedef struct {
    ForceFeedbackEffectState effects[FORCE_FEEDBACK_STATE_EFFECT_COUNT];
    bool primary_output_disabled;
    bool secondary_output_disabled;
} ForceFeedbackState;

void force_feedback_state_init(ForceFeedbackState *state);
bool force_feedback_state_apply(ForceFeedbackState *state, const ForceFeedbackCommand *command,
                                int32_t position_scale);

#endif
