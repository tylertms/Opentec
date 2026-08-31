#ifndef OPENTEC_BASE_SHIFTER_DISPLAY_H
#define OPENTEC_BASE_SHIFTER_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "shifter/calibration.h"
#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

typedef enum {
    SHIFTER_DISPLAY_WAITING,
    SHIFTER_DISPLAY_MONITORING,
    SHIFTER_DISPLAY_SHOWING,
} ShifterDisplayPhase;

typedef struct {
    ShifterDisplayPhase phase;
    ShifterGear last_gear;
    uint32_t clear_after_ms;
    bool calibration_visible;
    bool refresh_requested;
} ShifterDisplay;

void shifter_display_init(ShifterDisplay *display);
void shifter_display_request_refresh(ShifterDisplay *display);
bool shifter_display_update(ShifterDisplay *display, ShifterGear gear, bool wheel_active,
                            HPatternCalibrationPrompt calibration_prompt,
                            HPatternCalibrationPosition calibration_position, uint32_t now_ms,
                            WheelDisplayOutput *output);

#endif
