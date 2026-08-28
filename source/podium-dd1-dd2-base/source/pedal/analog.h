#ifndef OPENTEC_BASE_PEDAL_ANALOG_H
#define OPENTEC_BASE_PEDAL_ANALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/axis.h"
#include "pedal/input.h"

typedef struct {
    PedalAxisCalibration axes[PEDAL_INPUT_AXIS_COUNT];
    bool active;
} PedalAnalog;

void pedal_analog_init(PedalAnalog *analog);
bool pedal_analog_update(PedalAnalog *analog, const uint16_t samples[PEDAL_INPUT_AXIS_COUNT],
                         PedalInput *input);

#endif
