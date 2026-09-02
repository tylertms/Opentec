#ifndef OPENTEC_BASE_SHIFTER_INPUT_H
#define OPENTEC_BASE_SHIFTER_INPUT_H

#include <stdbool.h>
#include <stdint.h>

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

/**
 * @brief Identifies the first active contact in one sequential shifter pair.
 */
typedef enum {
    SHIFTER_SEQUENTIAL_NONE = 0,   /**< Neither contact is active. */
    SHIFTER_SEQUENTIAL_FIRST = 1,  /**< The first contact has priority. */
    SHIFTER_SEQUENTIAL_SECOND = 2, /**< The second contact is active. */
} ShifterSequentialPairState;

/**
 * @brief Applies official first-contact priority to a sequential pair.
 *
 * The firmware samples the first contact before the second and reports only the first active
 * contact when both active-low switches are asserted in one pair.
 *
 * @param[in] first_active True when the first contact is active.
 * @param[in] second_active True when the second contact is active.
 * @return Selected contact state.
 */
ShifterSequentialPairState shifter_sequential_pair_state(bool first_active, bool second_active);

#endif
