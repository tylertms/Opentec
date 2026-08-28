#include <assert.h>
#include <stdbool.h>

#include "force_feedback/output_enable.h"

static void test_prompt_waits_for_prerequisites_and_queue(void) {
    ForceOutputEnable enable = {0};
    ForceOutputEnableAction action;

    assert(force_output_enable_service(&enable, false, true, true, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_NONE);
    assert(force_output_enable_service(&enable, true, false, true, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_NONE);
    assert(force_output_enable_service(&enable, true, true, false, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_NONE);
    assert(enable.phase == FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED);

    assert(force_output_enable_service(&enable, true, true, true, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_SHOW_PROMPT);
    assert(enable.phase == FORCE_OUTPUT_ENABLE_AWAITING_CONFIRMATION);
}

static void test_only_response_one_releases_output(void) {
    ForceOutputEnable enable = {0};
    ForceOutputEnableAction action;

    assert(force_output_enable_service(&enable, true, true, true, &action));
    force_output_enable_set_response(&enable, 2);
    assert(force_output_enable_service(&enable, true, true, true, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_NONE);
    assert(enable.response == 2);

    force_output_enable_set_response(&enable, 1);
    assert(!force_output_enable_service(&enable, true, true, false, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_DISMISS_PROMPT);
    assert(enable.response == 0);
    assert(enable.phase == FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED);
}

static void test_lost_prerequisite_cancels_when_queue_is_available(void) {
    ForceOutputEnable enable = {0};
    ForceOutputEnableAction action;

    assert(force_output_enable_service(&enable, true, true, true, &action));
    assert(force_output_enable_service(&enable, false, true, false, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_NONE);
    assert(enable.phase == FORCE_OUTPUT_ENABLE_AWAITING_CONFIRMATION);

    assert(force_output_enable_service(&enable, false, true, true, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_CANCEL_PROMPT);
    assert(enable.phase == FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED);

    assert(force_output_enable_service(&enable, true, true, true, &action));
    assert(force_output_enable_service(&enable, true, false, true, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_CANCEL_PROMPT);
    assert(enable.phase == FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED);
}

static void test_unknown_phase_remains_interlocked(void) {
    ForceOutputEnable enable = {.phase = (ForceOutputEnablePhase)10};
    ForceOutputEnableAction action;

    assert(force_output_enable_service(&enable, true, true, true, &action));
    assert(action == FORCE_OUTPUT_ENABLE_ACTION_NONE);
}

int main(void) {
    test_prompt_waits_for_prerequisites_and_queue();
    test_only_response_one_releases_output();
    test_lost_prerequisite_cancels_when_queue_is_available();
    test_unknown_phase_remains_interlocked();
    return 0;
}
