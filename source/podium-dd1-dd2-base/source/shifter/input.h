#ifndef OPENTEC_BASE_SHIFTER_INPUT_H
#define OPENTEC_BASE_SHIFTER_INPUT_H

#include <stdbool.h>

/**
 * @brief Selects the shifter input protocol.
 *
 * Identifies the H-pattern and sequential input sources available to the base.
 */
typedef enum {
    SHIFTER_INPUT_H_PATTERN = 1,  /**< H-pattern shifter input is selected. */
    SHIFTER_INPUT_SEQUENTIAL = 2, /**< Sequential shifter input is selected. */
} ShifterInputMode;

/**
 * @brief Stores the selected shifter modes and transition flags.
 *
 * Each mode identifies the primary or secondary shifter input, and each transition flag reports a
 * change in that input's selected mode.
 */
typedef struct {
    ShifterInputMode primary_mode;   /**< Selected primary shifter input mode. */
    ShifterInputMode secondary_mode; /**< Selected secondary shifter input mode. */
    bool primary_transition;   /**< True when the primary mode changed during the current update. */
    bool secondary_transition; /**< True when the secondary mode changed during the current update.
                                */
} ShifterInputState;

#endif
