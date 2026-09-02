#include "shifter/input.h"

/**
 * @brief Selects the first active contact in a sequential pair.
 *
 * Reports the first contact when both switches are active, matching the binary's ordered
 * first-contact test.
 *
 * @param[in] first_active True when the first contact is active.
 * @param[in] second_active True when the second contact is active.
 * @return Selected contact state.
 */
ShifterSequentialPairState shifter_sequential_pair_state(bool first_active, bool second_active) {
    if (first_active) {
        return SHIFTER_SEQUENTIAL_FIRST;
    }
    return second_active ? SHIFTER_SEQUENTIAL_SECOND : SHIFTER_SEQUENTIAL_NONE;
}
