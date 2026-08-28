#include <assert.h>
#include <stdint.h>

#include "force_feedback/script_scheduler.h"

static void test_schedules_idle_ticks_after_deadline(void) {
    ForceFeedbackScriptScheduler scheduler = {.deadline = 10};
    ForceFeedbackScriptInputs inputs = {.status = FORCE_FEEDBACK_SCRIPT_INPUT_POSITION};

    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 0, 10) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE);
    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 0, 11) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_IDLE);
    assert(scheduler.deadline == 11);
}

static void test_schedules_active_and_ready_host_ticks(void) {
    ForceFeedbackScriptScheduler scheduler = {.deadline = 20};
    ForceFeedbackScriptInputs inputs = {
        .status = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE,
        .deadline = 100,
        .sample_count = 5,
    };

    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 10, 20) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE);
    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 10, 21) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_HOST);
    assert(scheduler.deadline == 26);

    inputs.status = FORCE_FEEDBACK_SCRIPT_INPUT_READY;
    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 11, 27) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_HOST);
    assert(scheduler.deadline == 32);
}

static void test_reports_expiration_and_rejects_incomplete_input(void) {
    ForceFeedbackScriptScheduler scheduler = {0};
    ForceFeedbackScriptInputs inputs = {
        .status = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE,
        .deadline = 10,
        .sample_count = 1,
    };

    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 10, 1) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_EXPIRED);
    inputs.deadline = 0;
    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 0, 1) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE);
    inputs.deadline = 10;
    inputs.sample_count = 0;
    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 0, 1) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE);
    inputs.status = 2;
    inputs.sample_count = 1;
    assert(force_feedback_script_scheduler_step(&scheduler, &inputs, 0, 1) ==
           FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE);
}

int main(void) {
    test_schedules_idle_ticks_after_deadline();
    test_schedules_active_and_ready_host_ticks();
    test_reports_expiration_and_rejects_incomplete_input();
    return 0;
}
