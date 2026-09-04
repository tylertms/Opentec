#include "system/torque_key_prompt.h"

#include <assert.h>
#include <stdbool.h>

static void show_prompt(TorqueKeyPrompt *prompt) {
    assert(torque_key_prompt_service(prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_NONE);
    assert(prompt->phase == TORQUE_KEY_PROMPT_SHOW_REQUIRED);
    assert(torque_key_prompt_service(prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_SHOW_PROMPT);
    assert(prompt->phase == TORQUE_KEY_PROMPT_SHOW_REQUIRED);
    assert(torque_key_prompt_service(prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_SHOW_PROMPT);
    assert(prompt->phase == TORQUE_KEY_PROMPT_SHOW_REQUIRED);
    torque_key_prompt_accept_action(prompt, TORQUE_KEY_PROMPT_ACTION_SHOW_PROMPT);
    assert(prompt->phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION);
}

static void test_commits_only_after_action_acceptance(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    show_prompt(&prompt);

    torque_key_prompt_set_response(&prompt, true);
    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_ENABLE_TORQUE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION);
    assert(prompt.response_pending);
    torque_key_prompt_accept_action(&prompt, TORQUE_KEY_PROMPT_ACTION_ENABLE_TORQUE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_TORQUE_ENABLED);
    assert(!prompt.response_pending);
}

static void test_removal_dismissal_waits_for_queue_acceptance(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    show_prompt(&prompt);

    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_HIGH, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT);
    assert(prompt.phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION);
    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_HIGH, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT);
    assert(prompt.phase == TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION);
    torque_key_prompt_accept_action(&prompt,
                                   TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT);
    assert(prompt.phase == TORQUE_KEY_PROMPT_REDUCED_TORQUE);
}

static void test_removal_after_torque_enable_enters_reduced_torque(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    show_prompt(&prompt);

    torque_key_prompt_set_response(&prompt, true);
    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_ENABLE_TORQUE);
    torque_key_prompt_accept_action(&prompt, TORQUE_KEY_PROMPT_ACTION_ENABLE_TORQUE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_TORQUE_ENABLED);

    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_HIGH, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_DISMISS_CURRENT);
    assert(prompt.phase == TORQUE_KEY_PROMPT_TORQUE_ENABLED);
    torque_key_prompt_accept_action(&prompt, TORQUE_KEY_PROMPT_ACTION_DISMISS_CURRENT);
    assert(prompt.phase == TORQUE_KEY_PROMPT_REDUCED_TORQUE);
}

static void test_revocation_runs_dismiss_and_reduced_phases(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    show_prompt(&prompt);

    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, true, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT);
    assert(prompt.phase == TORQUE_KEY_PROMPT_DISMISS_REQUIRED);
    torque_key_prompt_accept_action(&prompt,
                                   TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT);
    assert(prompt.phase == TORQUE_KEY_PROMPT_SHOW_REDUCED_REQUIRED);

    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, false, false, true) ==
           TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_STEERING_WHEEL);
    assert(prompt.phase == TORQUE_KEY_PROMPT_SHOW_REDUCED_REQUIRED);
    torque_key_prompt_accept_action(&prompt,
                                   TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_STEERING_WHEEL);
    assert(prompt.phase == TORQUE_KEY_PROMPT_REDUCED_TORQUE);
}

static void test_reduced_torque_waits_for_release_and_gates(void) {
    TorqueKeyPrompt prompt = {.phase = TORQUE_KEY_PROMPT_REDUCED_TORQUE};

    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_HIGH, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_NONE);
    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, true, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_NONE);
    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, false, true, false) ==
           TORQUE_KEY_PROMPT_ACTION_NONE);
    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_DISMISS_REDUCED_TORQUE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_REDUCED_TORQUE);
    torque_key_prompt_accept_action(&prompt, TORQUE_KEY_PROMPT_ACTION_DISMISS_REDUCED_TORQUE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_SHOW_REQUIRED);
}

static void test_selects_quick_release_reduced_torque(void) {
    TorqueKeyPrompt prompt = {.phase = TORQUE_KEY_PROMPT_SHOW_REDUCED_REQUIRED};

    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_QUICK_RELEASE);
    torque_key_prompt_accept_action(&prompt,
                                   TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_QUICK_RELEASE);
    assert(prompt.phase == TORQUE_KEY_PROMPT_REDUCED_TORQUE);
}

static void test_ignores_responses_outside_confirmation(void) {
    TorqueKeyPrompt prompt;
    torque_key_prompt_init(&prompt);
    torque_key_prompt_set_response(&prompt, true);
    assert(!prompt.response_pending);

    prompt.phase = (TorqueKeyPromptPhase)10;
    assert(torque_key_prompt_service(&prompt, TORQUE_KEY_INPUT_LOW, false, false, false) ==
           TORQUE_KEY_PROMPT_ACTION_NONE);
}

int main(void) {
    test_commits_only_after_action_acceptance();
    test_removal_dismissal_waits_for_queue_acceptance();
    test_removal_after_torque_enable_enters_reduced_torque();
    test_revocation_runs_dismiss_and_reduced_phases();
    test_reduced_torque_waits_for_release_and_gates();
    test_selects_quick_release_reduced_torque();
    test_ignores_responses_outside_confirmation();
    return 0;
}
