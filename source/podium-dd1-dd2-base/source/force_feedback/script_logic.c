#include "force_feedback/script_logic.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Tests a script value for ordered nonzero truth.
 *
 * Positive and negative ordered values, including infinities, are true. Both zeros and NaN are
 * false.
 *
 * @param[in] value Floating-point script value.
 * @return true when the value is ordered and nonzero; otherwise false.
 */
static bool ordered_nonzero(float value) {
    uint32_t representation;
    memcpy(&representation, &value, sizeof(representation));
    uint32_t magnitude = representation & UINT32_C(0x7fffffff);
    return magnitude != 0 && magnitude <= UINT32_C(0x7f800000);
}

/**
 * @brief Converts a C truth value to the script truth representation.
 *
 * Produces canonical floating-point one or zero.
 *
 * @param[in] value Truth value to convert.
 * @return Floating-point one for true or zero for false.
 */
static float logical_value(bool value) { return value ? 1.0f : 0.0f; }

float force_feedback_script_logic_evaluate(ForceFeedbackScriptLogicOperation operation, float first,
                                           float second) {
    bool first_value = ordered_nonzero(first);
    bool second_value = ordered_nonzero(second);

    switch (operation) {
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_AND:
        return logical_value(first_value && second_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_OR:
        return logical_value(first_value || second_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_NAND:
        return logical_value(!(first_value && second_value));
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_NOR:
        return logical_value(!(first_value || second_value));
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_XOR:
        return logical_value(first_value != second_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT:
        return logical_value(!first_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR:
        return logical_value(first_value == second_value);
    default:
        return 0.0f;
    }
}
