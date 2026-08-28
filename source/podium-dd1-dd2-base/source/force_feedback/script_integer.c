#include "force_feedback/script_integer.h"

#include <math.h>
#include <stdint.h>

static const float script_pi = 3.1415927f;

typedef union {
    float number;
    uint32_t bits;
} ScriptScalar;

static uint32_t float_bits(float value) { return (ScriptScalar){.number = value}.bits; }

static float bits_float(uint32_t bits) { return (ScriptScalar){.bits = bits}.number; }

static ForceFeedbackScriptIntegerResult value_result(uint32_t value) {
    return (ForceFeedbackScriptIntegerResult){.value = value, .writes_value = true};
}

/**
 * @brief Evaluate an integer-oriented script operation.
 *
 * Operands and results use the raw 32-bit script-slot representation. Opcodes 0xD0 and 0xD6
 * convert an unsigned integer to a float payload. Opcode 0xD1 truncates a float payload to an
 * unsigned integer when it is at most the float encoded by 0x4DCCCCCD; larger values suppress the
 * write, negative values write UINT32_MAX, and NaN writes 0x80000000. Opcode 0xD2 converts both
 * signed operands to float before subtracting. Opcode 0xD3 returns the unsigned absolute
 * difference. Opcodes 0xD4 and 0xD5 calculate first modulo second and suppress the write when the
 * divisor is zero; 0xD4 converts the remainder to a float payload. Degree conversion evaluates
 * ((float)first * pi) / 180 with pi encoded by 0x40490FDB.
 *
 * @param[in] operation Integer opcode from 0xD0 through 0xD6.
 * @param[in] first First or only raw operand.
 * @param[in] second Second raw operand for binary operations.
 * @return The raw destination payload and whether the VM writes it.
 * @pre operation is a defined ForceFeedbackScriptIntegerOperation value.
 */
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
            return value_result(UINT32_C(0x80000000));
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
