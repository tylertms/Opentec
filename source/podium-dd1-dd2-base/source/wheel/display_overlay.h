#ifndef OPENTEC_BASE_WHEEL_DISPLAY_OVERLAY_H
#define OPENTEC_BASE_WHEEL_DISPLAY_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

typedef enum {
    WHEEL_DISPLAY_OVERLAY_IDLE,
    WHEEL_DISPLAY_OVERLAY_HOLD_LABEL,
    WHEEL_DISPLAY_OVERLAY_COUNTDOWN,
    WHEEL_DISPLAY_OVERLAY_COMMAND,
} WheelDisplayOverlayPhase;

typedef struct {
    WheelDisplayOutput output;
    uint32_t hold_until_ms;
    uint32_t deadline_ms;
    uint8_t command;
    uint8_t remaining_seconds;
    WheelDisplayOverlayPhase phase;
    bool active;
} WheelDisplayOverlay;

void wheel_display_overlay_init(WheelDisplayOverlay *overlay);
void wheel_display_overlay_begin(WheelDisplayOverlay *overlay, uint8_t command, uint32_t now_ms);
bool wheel_display_overlay_update(WheelDisplayOverlay *overlay, uint32_t now_ms);

#endif
