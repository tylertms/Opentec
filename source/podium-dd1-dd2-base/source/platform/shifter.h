#ifndef OPENTEC_BASE_PLATFORM_SHIFTER_H
#define OPENTEC_BASE_PLATFORM_SHIFTER_H

#include "shifter/input.h"

void platform_shifter_init(void);
void platform_shifter_read(ShifterInputState *state);

#endif
