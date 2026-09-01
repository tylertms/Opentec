#include "system/torque_key_prompt.h"

#include <stdbool.h>

void torque_key_prompt_init(TorqueKeyPrompt *prompt) { *prompt = (TorqueKeyPrompt){0}; }

void torque_key_prompt_set_inserted(TorqueKeyPrompt *prompt, bool inserted) {
    if (inserted) {
        if (prompt->phase == TORQUE_KEY_PROMPT_REMOVED) {
            prompt->phase = TORQUE_KEY_PROMPT_SHOW_REQUIRED;
        } else if (prompt->phase == TORQUE_KEY_PROMPT_CANCEL_REQUIRED) {
            prompt->phase = TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION;
        }
        return;
    }

    prompt->response_pending = false;
    if (prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION) {
        prompt->phase = TORQUE_KEY_PROMPT_CANCEL_REQUIRED;
    } else {
        prompt->phase = TORQUE_KEY_PROMPT_REMOVED;
    }
}

void torque_key_prompt_set_response(TorqueKeyPrompt *prompt, bool accepted) {
    if (accepted && prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION) {
        prompt->response_pending = true;
    }
}

TorqueKeyPromptAction torque_key_prompt_service(TorqueKeyPrompt *prompt,
                                                bool event_slot_available) {
    if (prompt->phase == TORQUE_KEY_PROMPT_SHOW_REQUIRED && event_slot_available) {
        prompt->phase = TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION;
        return TORQUE_KEY_PROMPT_ACTION_SHOW;
    }
    if (prompt->phase == TORQUE_KEY_PROMPT_CANCEL_REQUIRED && event_slot_available) {
        prompt->phase = TORQUE_KEY_PROMPT_REMOVED;
        return TORQUE_KEY_PROMPT_ACTION_CANCEL;
    }
    if (prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION && prompt->response_pending) {
        prompt->response_pending = false;
        prompt->phase = TORQUE_KEY_PROMPT_ACKNOWLEDGED;
        return TORQUE_KEY_PROMPT_ACTION_DISMISS;
    }
    return TORQUE_KEY_PROMPT_ACTION_NONE;
}
