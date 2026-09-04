#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "display/reset_scheduler.h"

static void test_waits_with_strict_reference_deadlines(void) {
    DisplayResetScheduler scheduler;
    display_reset_scheduler_init(&scheduler);

    assert(display_reset_scheduler_step(&scheduler, 100) == DISPLAY_RESET_ACTION_NONE);
    assert(scheduler.phase == DISPLAY_RESET_PHASE_ASSERT);
    assert(display_reset_scheduler_step(&scheduler, 102) == DISPLAY_RESET_ACTION_NONE);
    assert(display_reset_scheduler_step(&scheduler, 103) == DISPLAY_RESET_ACTION_ASSERT_LOW);
    assert(scheduler.phase == DISPLAY_RESET_PHASE_RELEASE);
    assert(display_reset_scheduler_step(&scheduler, 104) == DISPLAY_RESET_ACTION_NONE);
    assert(display_reset_scheduler_step(&scheduler, 105) == DISPLAY_RESET_ACTION_RELEASE_HIGH);
    assert(scheduler.phase == DISPLAY_RESET_PHASE_COMPLETE);
    assert(display_reset_scheduler_step(&scheduler, 106) == DISPLAY_RESET_ACTION_NONE);
    assert(display_reset_scheduler_step(&scheduler, 107) == DISPLAY_RESET_ACTION_NONE);
    assert(scheduler.phase == DISPLAY_RESET_PHASE_SETUP);
    assert(display_reset_scheduler_step(&scheduler, 107) == DISPLAY_RESET_ACTION_NONE);
}

static void test_ignores_null_scheduler(void) {
    assert(display_reset_scheduler_step(NULL, 0) == DISPLAY_RESET_ACTION_NONE);
    display_reset_scheduler_init(NULL);
}

static void test_restarts_after_completion(void) {
    DisplayResetScheduler scheduler;
    display_reset_scheduler_init(&scheduler);

    assert(display_reset_scheduler_step(&scheduler, 200) == DISPLAY_RESET_ACTION_NONE);
    assert(display_reset_scheduler_step(&scheduler, 203) == DISPLAY_RESET_ACTION_ASSERT_LOW);
    assert(display_reset_scheduler_step(&scheduler, 203) == DISPLAY_RESET_ACTION_NONE);
    assert(display_reset_scheduler_step(&scheduler, 205) == DISPLAY_RESET_ACTION_RELEASE_HIGH);
    assert(display_reset_scheduler_step(&scheduler, 207) == DISPLAY_RESET_ACTION_NONE);

    assert(display_reset_scheduler_step(&scheduler, 207) == DISPLAY_RESET_ACTION_NONE);
    assert(display_reset_scheduler_step(&scheduler, 209) == DISPLAY_RESET_ACTION_NONE);
    assert(display_reset_scheduler_step(&scheduler, 210) == DISPLAY_RESET_ACTION_ASSERT_LOW);
}

int main(void) {
    test_waits_with_strict_reference_deadlines();
    test_ignores_null_scheduler();
    test_restarts_after_completion();
    return 0;
}
