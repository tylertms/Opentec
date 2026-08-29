#include "system/torque_key_prompt.h"

#include <stdbool.h>

/**
 * @brief Initializes Torque Key acknowledgement policy.
 *
 * Starts in the removed state without a pending operator response.
 *
 * @param[out] prompt Torque Key prompt policy to initialize.
 */
void torque_key_prompt_init(TorqueKeyPrompt *prompt) { *prompt = (TorqueKeyPrompt){0}; }

/**
 * @brief Applies a stable Torque Key presence change.
 *
 * Insertion requests a fresh acknowledgement. Removal cancels a displayed prompt, drops a prompt
 * that has not yet acquired the event slot, or returns an acknowledged key to the removed state.
 * Reinsertion before a queued cancellation completes resumes the displayed prompt.
 *
 * @param[in,out] prompt Torque Key prompt policy.
 * @param[in] inserted True when the Torque Key is stably inserted.
 */
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

/**
 * @brief Stores an accepted Torque Key safety response.
 *
 * Retains response value one only while the acknowledgement prompt is active. Other response
 * values and responses outside that phase do not advance the policy.
 *
 * @param[in,out] prompt Torque Key prompt policy.
 * @param[in] accepted True for the accepted response value.
 */
void torque_key_prompt_set_response(TorqueKeyPrompt *prompt, bool accepted) {
    if (accepted && prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION) {
        prompt->response_pending = true;
    }
}

/**
 * @brief Advances Torque Key prompt presentation through the shared event slot.
 *
 * Show and cancellation transitions wait until the event slot is available. An accepted response
 * dismisses the prompt immediately and leaves the inserted key acknowledged until it is removed.
 *
 * @param[in,out] prompt Torque Key prompt policy.
 * @param[in] event_slot_available True when a presentation event can be accepted.
 * @return Prompt presentation action for the firmware integration layer.
 */
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
