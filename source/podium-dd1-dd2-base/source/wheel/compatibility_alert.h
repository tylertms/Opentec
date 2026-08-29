#ifndef OPENTEC_BASE_WHEEL_COMPATIBILITY_ALERT_H
#define OPENTEC_BASE_WHEEL_COMPATIBILITY_ALERT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WHEEL_COMPATIBILITY_ALERT_ACTION_NONE,
    WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED,
    WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_OUTLINED,
    WHEEL_COMPATIBILITY_ALERT_ACTION_CLEAR,
} WheelCompatibilityAlertAction;

typedef struct {
    uint32_t next_toggle_ms;
    bool active;
    bool outlined;
    bool presentation_pending;
} WheelCompatibilityAlert;

void wheel_compatibility_alert_init(WheelCompatibilityAlert *alert);
WheelCompatibilityAlertAction wheel_compatibility_alert_update(WheelCompatibilityAlert *alert,
                                                               bool unsupported,
                                                               bool event_slot_available,
                                                               uint32_t now_ms);
bool wheel_compatibility_alert_segment_visible(uint32_t now_ms);

#endif
