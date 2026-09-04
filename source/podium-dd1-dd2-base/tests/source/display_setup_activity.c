#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "display/setup_activity.h"

static void starts_when_either_raw_input_is_active(void) {
    DisplaySetupActivity activity;
    display_setup_activity_init(&activity);

    assert(!display_setup_activity_update(&activity, false, false, 100u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_IDLE);
    assert(display_setup_activity_update(&activity, true, false, 100u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST);

    display_setup_activity_init(&activity);
    assert(display_setup_activity_update(&activity, false, true, 100u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST);
}

static void waits_for_strict_deadline(void) {
    DisplaySetupActivity activity = {
        .deadline_ms = 500u,
        .phase = DISPLAY_SETUP_ACTIVITY_FIRST,
    };

    assert(display_setup_activity_update(&activity, true, false, 500u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST);
    assert(activity.revision == 0u);
    assert(display_setup_activity_update(&activity, true, false, 501u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST + 1u);
    assert(activity.text_phase == DISPLAY_SETUP_ACTIVITY_FIRST);
    assert(activity.revision == 1u);
    assert(activity.deadline_ms == 1001u);
}

static void clears_after_first_frame_when_inactive(void) {
    DisplaySetupActivity activity = {
        .deadline_ms = 500u,
        .phase = DISPLAY_SETUP_ACTIVITY_FIRST,
    };

    assert(!display_setup_activity_update(&activity, false, false, 501u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_IDLE);
    assert(activity.text_phase == DISPLAY_SETUP_ACTIVITY_FIRST);
    assert(activity.revision == 1u);
}

static void advances_each_activity_frame(void) {
    DisplaySetupActivity activity = {
        .deadline_ms = 100u,
        .phase = DISPLAY_SETUP_ACTIVITY_FIRST + 1u,
    };

    assert(display_setup_activity_update(&activity, false, true, 101u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST + 2u);
    assert(activity.text_phase == DISPLAY_SETUP_ACTIVITY_FIRST + 1u);
    assert(activity.revision == 1u);
    assert(activity.deadline_ms == 601u);
    assert(display_setup_activity_update(&activity, false, true, 602u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST + 3u);
    assert(activity.text_phase == DISPLAY_SETUP_ACTIVITY_FIRST + 2u);
    assert(activity.revision == 2u);
    assert(activity.deadline_ms == 1102u);
    assert(display_setup_activity_update(&activity, false, true, 1103u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_RESTART);
    assert(activity.text_phase == DISPLAY_SETUP_ACTIVITY_FIRST + 3u);
    assert(activity.revision == 3u);
    assert(activity.deadline_ms == 1603u);
}

static void waits_before_restarting_phase_five(void) {
    DisplaySetupActivity activity = {
        .deadline_ms = 700u,
        .phase = DISPLAY_SETUP_ACTIVITY_RESTART,
    };

    assert(!display_setup_activity_update(&activity, false, false, 700u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_RESTART);
    assert(!display_setup_activity_update(&activity, false, false, 701u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST);
}

static void retains_raw_unsigned_deadline_semantics(void) {
    DisplaySetupActivity activity = {
        .deadline_ms = UINT32_MAX,
        .phase = DISPLAY_SETUP_ACTIVITY_FIRST,
    };

    assert(display_setup_activity_update(&activity, true, false, UINT32_MAX));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST);
    assert(display_setup_activity_update(&activity, true, false, 0u));
    assert(activity.phase == DISPLAY_SETUP_ACTIVITY_FIRST);
}

static void ignores_null_state(void) {
    display_setup_activity_init(NULL);
    assert(!display_setup_activity_update(NULL, true, true, 1u));
}

int main(void) {
    starts_when_either_raw_input_is_active();
    waits_for_strict_deadline();
    clears_after_first_frame_when_inactive();
    advances_each_activity_frame();
    waits_before_restarting_phase_five();
    retains_raw_unsigned_deadline_semantics();
    ignores_null_state();
    return 0;
}
