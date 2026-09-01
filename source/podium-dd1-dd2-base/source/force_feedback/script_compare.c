#include "force_feedback/script_compare.h"

#include <stdbool.h>

/**
 * @brief Converts a comparison predicate to its script float value.
 *
 * Produces the canonical single-precision values 1.0 for true and 0.0 for false.
 *
 * @param[in] value Predicate result to convert.
 * @return Float 1.0 when true, otherwise float 0.0.
 */
static float comparison_value(bool value) { return value ? 1.0f : 0.0f; }

float force_feedback_script_compare(ForceFeedbackScriptComparison comparison, float first,
                                    float second) {
    switch (comparison) {
    case FORCE_FEEDBACK_SCRIPT_GREATER_THAN:
        return comparison_value(first > second);
    case FORCE_FEEDBACK_SCRIPT_GREATER_OR_EQUAL:
        return comparison_value(first >= second);
    case FORCE_FEEDBACK_SCRIPT_LESS_THAN:
        return comparison_value(first < second);
    case FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL:
        return comparison_value(first <= second);
    case FORCE_FEEDBACK_SCRIPT_NEGATIVE:
        return comparison_value(first < 0.0f);
    case FORCE_FEEDBACK_SCRIPT_POSITIVE:
        return comparison_value(first > 0.0f);
    default:
        return 0.0f;
    }
}
