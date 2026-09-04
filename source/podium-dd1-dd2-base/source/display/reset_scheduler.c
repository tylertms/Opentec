#include "display/reset_scheduler.h"

#include <stddef.h>

/** @brief Reference display reset delays in milliseconds. */
enum {
    DISPLAY_RESET_SETUP_DELAY_MS = 2,
    DISPLAY_RESET_LOW_DELAY_MS = 1,
    DISPLAY_RESET_RECOVERY_DELAY_MS = 1,
};

void display_reset_scheduler_init(DisplayResetScheduler *scheduler) {
    if (scheduler != NULL) {
        *scheduler = (DisplayResetScheduler){.phase = DISPLAY_RESET_PHASE_SETUP};
    }
}

DisplayResetAction display_reset_scheduler_step(DisplayResetScheduler *scheduler, uint32_t now_ms) {
    if (scheduler == NULL) {
        return DISPLAY_RESET_ACTION_NONE;
    }

    switch (scheduler->phase) {
    case DISPLAY_RESET_PHASE_SETUP:
        scheduler->deadline_ms = now_ms + DISPLAY_RESET_SETUP_DELAY_MS;
        scheduler->phase = DISPLAY_RESET_PHASE_ASSERT;
        return DISPLAY_RESET_ACTION_NONE;
    case DISPLAY_RESET_PHASE_ASSERT:
        if (now_ms <= scheduler->deadline_ms) {
            return DISPLAY_RESET_ACTION_NONE;
        }
        scheduler->deadline_ms = now_ms + DISPLAY_RESET_LOW_DELAY_MS;
        scheduler->phase = DISPLAY_RESET_PHASE_RELEASE;
        return DISPLAY_RESET_ACTION_ASSERT_LOW;
    case DISPLAY_RESET_PHASE_RELEASE:
        if (now_ms <= scheduler->deadline_ms) {
            return DISPLAY_RESET_ACTION_NONE;
        }
        scheduler->deadline_ms = now_ms + DISPLAY_RESET_RECOVERY_DELAY_MS;
        scheduler->phase = DISPLAY_RESET_PHASE_COMPLETE;
        return DISPLAY_RESET_ACTION_RELEASE_HIGH;
    case DISPLAY_RESET_PHASE_COMPLETE:
        if (now_ms <= scheduler->deadline_ms) {
            return DISPLAY_RESET_ACTION_NONE;
        }
        scheduler->phase = DISPLAY_RESET_PHASE_SETUP;
        return DISPLAY_RESET_ACTION_NONE;
    default:
        return DISPLAY_RESET_ACTION_NONE;
    }
}
