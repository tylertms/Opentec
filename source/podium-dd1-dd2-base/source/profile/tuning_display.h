#ifndef OPENTEC_BASE_PROFILE_TUNING_DISPLAY_H
#define OPENTEC_BASE_PROFILE_TUNING_DISPLAY_H

#include <stdbool.h>

#include "profile/bank.h"
#include "profile/tuning_menu.h"
#include "wheel/display_output.h"

bool tuning_display_render(const TuningMenu *menu, const TuningProfileBank *bank,
                           WheelDisplayOutput *output);

#endif
