#ifndef OPENTEC_BASE_WHEEL_STARTUP_DISPLAY_H
#define OPENTEC_BASE_WHEEL_STARTUP_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "wheel/display_output.h"

typedef enum {
    WHEEL_STARTUP_DISPLAY_DASHES,
    WHEEL_STARTUP_DISPLAY_BASE_VERSION,
    WHEEL_STARTUP_DISPLAY_MOTOR_VERSION,
    WHEEL_STARTUP_DISPLAY_READY_DELAY,
    WHEEL_STARTUP_DISPLAY_CALIBRATION,
    WHEEL_STARTUP_DISPLAY_CALIBRATION_PAUSE,
    WHEEL_STARTUP_DISPLAY_COMPLETE,
} WheelStartupDisplayPhase;

typedef struct {
    WheelStartupDisplayPhase phase;
    uint32_t deadline_ms;
    uint32_t version_presentation_close_ms;
    bool ready;
    bool version_presentation_pending;
    bool version_presentation_close_armed;
    bool version_presentation_close_pending;
} WheelStartupDisplay;

void wheel_startup_display_init(WheelStartupDisplay *display);
bool wheel_startup_display_update(WheelStartupDisplay *display, bool wheel_active,
                                  bool tuning_display_supported, bool position_ready,
                                  const MotorIdentity *motor_identity, uint32_t now_ms,
                                  WheelDisplayOutput *output);
bool wheel_startup_display_ready(const WheelStartupDisplay *display);
bool wheel_startup_display_take_version_presentation(WheelStartupDisplay *display);
bool wheel_startup_display_take_version_presentation_close(WheelStartupDisplay *display);

#endif
