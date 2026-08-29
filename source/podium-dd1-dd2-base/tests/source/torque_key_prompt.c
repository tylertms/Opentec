#include "system/torque_key_prompt.h"

#include <assert.h>
#include <stdbool.h>

static void test_waits_for_event_slot_and_acknowledgement(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    torque_key_prompt_set_inserted(&prompt, true);

    assert(torque_key_prompt_service(&prompt, false) == TORQUE_KEY_PROMPT_ACTION_NONE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_SHOW_REQUIRED);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_SHOW);
    assert(prompt.phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION);
    torque_key_prompt_set_response(&prompt, true);
    assert(torque_key_prompt_service(&prompt, false) == TORQUE_KEY_PROMPT_ACTION_DISMISS);
    assert(prompt.phase == TORQUE_KEY_PROMPT_ACKNOWLEDGED);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_NONE);
}

static void test_ignores_other_responses(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    torque_key_prompt_set_response(&prompt, true);
    torque_key_prompt_set_inserted(&prompt, true);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_SHOW);

    torque_key_prompt_set_response(&prompt, false);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_NONE);
}

static void test_cancels_prompt_after_removal(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    torque_key_prompt_set_inserted(&prompt, true);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_SHOW);

    torque_key_prompt_set_inserted(&prompt, false);
    assert(torque_key_prompt_service(&prompt, false) == TORQUE_KEY_PROMPT_ACTION_NONE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_CANCEL_REQUIRED);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_CANCEL);
    assert(prompt.phase == TORQUE_KEY_PROMPT_REMOVED);
}

static void test_handles_removal_around_queue_transitions(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    torque_key_prompt_set_inserted(&prompt, true);
    torque_key_prompt_set_inserted(&prompt, false);
    assert(prompt.phase == TORQUE_KEY_PROMPT_REMOVED);

    torque_key_prompt_set_inserted(&prompt, true);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_SHOW);
    torque_key_prompt_set_inserted(&prompt, false);
    torque_key_prompt_set_inserted(&prompt, true);
    assert(prompt.phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION);
    assert(torque_key_prompt_service(&prompt, true) == TORQUE_KEY_PROMPT_ACTION_NONE);
}

int main(void) {
    test_waits_for_event_slot_and_acknowledgement();
    test_ignores_other_responses();
    test_cancels_prompt_after_removal();
    test_handles_removal_around_queue_transitions();
    return 0;
}
