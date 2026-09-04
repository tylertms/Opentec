#include "display/setup_activity.h"

#include <stddef.h>

enum {
    DISPLAY_SETUP_ACTIVITY_INTERVAL_MS = 500u,
};

void display_setup_activity_init(DisplaySetupActivity *activity) {
    if (activity != NULL) {
        *activity = (DisplaySetupActivity){0};
    }
}

bool display_setup_activity_update(DisplaySetupActivity *activity, bool pedal_handshake_active,
                                   bool non_fanatec_mode, uint32_t now_ms) {
    if (activity == NULL) {
        return false;
    }

    bool active = pedal_handshake_active || non_fanatec_mode;
    if (activity->phase == DISPLAY_SETUP_ACTIVITY_IDLE) {
        if (active) {
            activity->phase = DISPLAY_SETUP_ACTIVITY_FIRST;
        }
        return active;
    }
    if (activity->phase == DISPLAY_SETUP_ACTIVITY_RESTART) {
        if (now_ms > activity->deadline_ms) {
            activity->phase = DISPLAY_SETUP_ACTIVITY_FIRST;
        }
        return active;
    }
    if (activity->phase > DISPLAY_SETUP_ACTIVITY_LAST || now_ms <= activity->deadline_ms) {
        return active;
    }

    activity->text_phase = activity->phase;
    activity->revision++;
    if (activity->phase == DISPLAY_SETUP_ACTIVITY_FIRST && !active) {
        activity->phase = DISPLAY_SETUP_ACTIVITY_IDLE;
    } else {
        activity->deadline_ms = now_ms + DISPLAY_SETUP_ACTIVITY_INTERVAL_MS;
        activity->phase++;
    }
    return active;
}
