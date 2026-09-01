#ifndef OPENTEC_BASE_PLATFORM_SHIFTER_H
#define OPENTEC_BASE_PLATFORM_SHIFTER_H

#include "shifter/input.h"

/**
 * @brief Initializes and identifies both shifter inputs.
 *
 * Configures the identification and signal pins, then selects H-pattern or sequential input
 * handling for each connected shifter.
 */
void platform_shifter_init(void);

/**
 * @brief Reads both shifter input states.
 *
 * Updates mode detection and reports sequential transition inputs in the supplied state object.
 *
 * @param[out] state Destination for the primary and secondary shifter state.
 */
void platform_shifter_read(ShifterInputState *state);

#endif
