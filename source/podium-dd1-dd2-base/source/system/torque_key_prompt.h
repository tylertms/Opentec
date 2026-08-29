#ifndef OPENTEC_BASE_SYSTEM_TORQUE_KEY_PROMPT_H
#define OPENTEC_BASE_SYSTEM_TORQUE_KEY_PROMPT_H

#include <stdbool.h>

typedef enum {
    TORQUE_KEY_PROMPT_REMOVED,
    TORQUE_KEY_PROMPT_SHOW_REQUIRED,
    TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION,
    TORQUE_KEY_PROMPT_CANCEL_REQUIRED,
    TORQUE_KEY_PROMPT_ACKNOWLEDGED,
} TorqueKeyPromptPhase;

typedef enum {
    TORQUE_KEY_PROMPT_ACTION_NONE,
    TORQUE_KEY_PROMPT_ACTION_SHOW,
    TORQUE_KEY_PROMPT_ACTION_CANCEL,
    TORQUE_KEY_PROMPT_ACTION_DISMISS,
} TorqueKeyPromptAction;

typedef struct {
    TorqueKeyPromptPhase phase;
    bool response_pending;
} TorqueKeyPrompt;

void torque_key_prompt_init(TorqueKeyPrompt *prompt);
void torque_key_prompt_set_inserted(TorqueKeyPrompt *prompt, bool inserted);
void torque_key_prompt_set_response(TorqueKeyPrompt *prompt, bool accepted);
TorqueKeyPromptAction torque_key_prompt_service(TorqueKeyPrompt *prompt, bool event_slot_available);

#endif
