#include "force_feedback/script_integer.h"

#include <math.h>
#include <stdint.h>

/**
 * @brief Pi constant used by the script degrees-to-radians conversion.
 *
 * The stored single-precision value is 3.1415927f.
 */
static const float script_pi = 3.1415927f;

/**
 * @brief Provides both representations of one script scalar.
 *
 * The evaluator uses the union to reinterpret a single-precision value without changing its
 * 32-bit payload.
 */
typedef union {
    float number;  /**< Single-precision interpretation of the payload. */
    uint32_t bits; /**< Raw 32-bit interpretation of the payload. */
} ScriptScalar;

/**
 * @brief Returns the raw representation of a script floating-point value.
 *
 * Preserves all bits of the single-precision value.
 *
 * @param[in] value Floating-point value.
 * @return The raw 32-bit representation.
 */
static uint32_t float_bits(float value) { return (ScriptScalar){.number = value}.bits; }

/**
 * @brief Interprets a raw script value as floating point.
 *
 * Preserves all bits while changing only the C representation used by the evaluator.
 *
 * @param[in] bits Raw 32-bit representation.
 * @return The represented single-precision value.
 */
static float bits_float(uint32_t bits) { return (ScriptScalar){.bits = bits}.number; }

/**
 * @brief Creates a writable integer-operation result.
 *
 * Marks the supplied raw value for delivery to the encoded destination.
 *
 * @param[in] value Raw operation result.
 * @return A writable integer-operation result containing the value.
 */
static ForceFeedbackScriptIntegerResult value_result(uint32_t value) {
    return (ForceFeedbackScriptIntegerResult){.value = value, .writes_value = true};
}

ForceFeedbackScriptIntegerResult
force_feedback_script_integer_evaluate(ForceFeedbackScriptIntegerOperation operation,
                                       uint32_t first, uint32_t second) {
    switch (operation) {
    case FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT:
        return value_result(float_bits((float)first));
    case FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32: {
        float value = bits_float(first);
        if (value > bits_float(UINT32_C(0x4dcccccd))) {
            return (ForceFeedbackScriptIntegerResult){0};
        }
        if (isnan(value)) {
            return value_result(0);
        }
        return value_result(value < 0.0f ? UINT32_MAX : (uint32_t)value);
    }
    case FORCE_FEEDBACK_SCRIPT_INTEGER_SUBTRACT_I32_TO_FLOAT:
        return value_result(float_bits((float)(int32_t)first - (float)(int32_t)second));
    case FORCE_FEEDBACK_SCRIPT_INTEGER_ABSOLUTE_DIFFERENCE:
        return value_result(first >= second ? first - second : second - first);
    case FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO_TO_FLOAT:
        return second == 0 ? (ForceFeedbackScriptIntegerResult){0}
                           : value_result(float_bits((float)(first % second)));
    case FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO:
        return second == 0 ? (ForceFeedbackScriptIntegerResult){0} : value_result(first % second);
    case FORCE_FEEDBACK_SCRIPT_INTEGER_DEGREES_TO_RADIANS:
        return value_result(float_bits((float)first * script_pi / 180.0f));
    default:
        return (ForceFeedbackScriptIntegerResult){0};
    }
}
