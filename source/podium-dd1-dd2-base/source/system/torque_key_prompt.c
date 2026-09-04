#include "system/torque_key_prompt.h"

#include <stdbool.h>

void torque_key_prompt_init(TorqueKeyPrompt *prompt) { *prompt = (TorqueKeyPrompt){0}; }

void torque_key_prompt_set_response(TorqueKeyPrompt *prompt, bool accepted) {
    if (accepted && prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION) {
        prompt->response_pending = true;
    }
}

TorqueKeyPromptAction torque_key_prompt_service(TorqueKeyPrompt *prompt,
                                                TorqueKeyInputState input,
                                                bool button_scan_pending,
                                                bool calibration_available,
                                                bool protocol_request_pending) {
    if ((button_scan_pending || calibration_available) &&
        (prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION ||
         prompt->phase == TORQUE_KEY_PROMPT_TORQUE_ENABLED)) {
        prompt->phase = TORQUE_KEY_PROMPT_DISMISS_REQUIRED;
    }

    switch (prompt->phase) {
    case TORQUE_KEY_PROMPT_IDLE:
        if (input == TORQUE_KEY_INPUT_LOW) {
            prompt->phase = TORQUE_KEY_PROMPT_SHOW_REQUIRED;
        } else if (input == TORQUE_KEY_INPUT_HIGH) {
            prompt->phase = TORQUE_KEY_PROMPT_REDUCED_TORQUE;
        }
        return TORQUE_KEY_PROMPT_ACTION_NONE;

    case TORQUE_KEY_PROMPT_SHOW_REQUIRED:
        return TORQUE_KEY_PROMPT_ACTION_SHOW_PROMPT;

    case TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION:
        if (input != TORQUE_KEY_INPUT_LOW) {
            return TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT;
        }
        if (prompt->response_pending) {
            return TORQUE_KEY_PROMPT_ACTION_ENABLE_TORQUE;
        }
        return TORQUE_KEY_PROMPT_ACTION_NONE;

    case TORQUE_KEY_PROMPT_TORQUE_ENABLED:
        return input == TORQUE_KEY_INPUT_LOW ? TORQUE_KEY_PROMPT_ACTION_NONE
                                             : TORQUE_KEY_PROMPT_ACTION_DISMISS_CURRENT;

    case TORQUE_KEY_PROMPT_REDUCED_TORQUE:
        if (input != TORQUE_KEY_INPUT_LOW || button_scan_pending || calibration_available) {
            return TORQUE_KEY_PROMPT_ACTION_NONE;
        }
        return TORQUE_KEY_PROMPT_ACTION_DISMISS_REDUCED_TORQUE;

    case TORQUE_KEY_PROMPT_DISMISS_REQUIRED:
        return TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT;

    case TORQUE_KEY_PROMPT_SHOW_REDUCED_REQUIRED:
        return protocol_request_pending || button_scan_pending
                   ? TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_STEERING_WHEEL
                   : TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_QUICK_RELEASE;
    }

    return TORQUE_KEY_PROMPT_ACTION_NONE;
}

void torque_key_prompt_accept_action(TorqueKeyPrompt *prompt, TorqueKeyPromptAction action) {
    switch (action) {
    case TORQUE_KEY_PROMPT_ACTION_SHOW_PROMPT:
        if (prompt->phase == TORQUE_KEY_PROMPT_SHOW_REQUIRED) {
            prompt->phase = TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION;
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT:
        if (prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION) {
            prompt->phase = TORQUE_KEY_PROMPT_REDUCED_TORQUE;
        } else if (prompt->phase == TORQUE_KEY_PROMPT_DISMISS_REQUIRED) {
            prompt->phase = TORQUE_KEY_PROMPT_SHOW_REDUCED_REQUIRED;
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_ENABLE_TORQUE:
        if (prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION &&
            prompt->response_pending) {
            prompt->response_pending = false;
            prompt->phase = TORQUE_KEY_PROMPT_TORQUE_ENABLED;
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_DISMISS_CURRENT:
        if (prompt->phase == TORQUE_KEY_PROMPT_TORQUE_ENABLED) {
            prompt->phase = TORQUE_KEY_PROMPT_REDUCED_TORQUE;
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_DISMISS_REDUCED_TORQUE:
        if (prompt->phase == TORQUE_KEY_PROMPT_REDUCED_TORQUE) {
            prompt->phase = TORQUE_KEY_PROMPT_SHOW_REQUIRED;
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_QUICK_RELEASE:
    case TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_STEERING_WHEEL:
        if (prompt->phase == TORQUE_KEY_PROMPT_SHOW_REDUCED_REQUIRED) {
            prompt->phase = TORQUE_KEY_PROMPT_REDUCED_TORQUE;
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_NONE:
        break;
    }
}
