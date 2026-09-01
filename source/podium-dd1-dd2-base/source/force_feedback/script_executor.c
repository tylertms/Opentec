#include "force_feedback/script_executor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operation.h"

/**
 * @brief Force-feedback script executor command bytes.
 */
enum {
    COMMAND_CURSOR = 0x00,          /**< Consumes a cursor command without further action. */
    COMMAND_ADVANCE = 0x01,         /**< Starts or preserves an unconditional suppression span. */
    COMMAND_ADVANCE_IF_ZERO = 0x02, /**< Advances when the condition is floating-point zero. */
    COMMAND_ADVANCE_IF_NONZERO =
        0x03, /**< Advances when the condition is not floating-point zero. */
    COMMAND_ENSURE_CURSOR = 0x04,
    COMMAND_REWIND_IF_ZERO = 0x05,
    COMMAND_REWIND_IF_NONZERO = 0x06,
    COMMAND_RESERVE_SLOT = 0x07,
    COMMAND_COMPLETE_IF_ZERO = 0x08, /**< Completes when the condition is floating-point zero. */
    COMMAND_COMPLETE_IF_NONZERO =
        0x09, /**< Completes when the condition is not floating-point zero. */
};

/**
 * @brief Provides floating-point and raw-bit views of one executor value.
 *
 * The executor uses the union to test raw script values as single-precision values without
 * converting their bit patterns.
 */
typedef union {
    float number;  /**< Single-precision interpretation of the value. */
    uint32_t bits; /**< Raw 32-bit interpretation of the value. */
} ExecutionValue;

/**
 * @brief Tracks the executor cursor, suppression state, and input validity.
 */
typedef struct {
    size_t cursor;      /**< Offset of the next command or operand to consume. */
    uint16_t remaining; /**< Internal suppression count still pending. */
    bool valid;         /**< Whether the most recently consumed encoding was valid. */
} ExecutionState;

/**
 * @brief Tests a raw script value for floating-point zero.
 *
 * Positive and negative zero both satisfy the comparison.
 *
 * @param[in] value Raw floating-point value.
 * @return true when the value compares equal to zero; otherwise false.
 */
static bool is_zero(uint32_t value) { return (ExecutionValue){.bits = value}.number == 0.0f; }

/**
 * @brief Starts a record-suppression span.
 *
 * Consumes the count operand and stores its low byte plus one when no suppression span is already
 * active.
 *
 * @param[in] runtime Script state referenced by the count operand.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] state Current execution cursor, suppression count, and validity state.
 * @return The updated execution state.
 */
static ExecutionState set_advance(const ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                                  size_t length, ExecutionState state) {
    ForceFeedbackScriptOperandResult count =
        force_feedback_script_operand_read(runtime, script, length, state.cursor);
    state.cursor = count.cursor;
    state.valid = count.valid;
    if (state.valid && state.remaining == 0) {
        state.remaining = (uint16_t)(uint8_t)count.value + 1u;
    }
    return state;
}

/**
 * @brief Selects or skips a conditional suppression count.
 *
 * Consumes a condition and then consumes the count operand on both branches. The selected branch
 * applies the count to suppression state; the other branch discards the decoded count.
 *
 * @param[in] runtime Script state referenced by both operands.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] state Current execution cursor, suppression count, and validity state.
 * @param[in] advance_when_zero true to select a zero condition; false to select a nonzero
 * condition.
 * @return The updated execution state.
 */
static ExecutionState conditional_advance(const ForceFeedbackScriptRuntime *runtime,
                                          const uint8_t *script, size_t length,
                                          ExecutionState state, bool advance_when_zero) {
    ForceFeedbackScriptOperandResult condition =
        force_feedback_script_operand_read(runtime, script, length, state.cursor);
    state.cursor = condition.cursor;
    state.valid = condition.valid;
    if (!state.valid) {
        return state;
    }
    if (is_zero(condition.value) == advance_when_zero) {
        return set_advance(runtime, script, length, state);
    }
    ForceFeedbackScriptOperandResult count =
        force_feedback_script_operand_read(runtime, script, length, state.cursor);
    state.cursor = count.cursor;
    state.valid = count.valid;
    return state;
}

/**
 * @brief Faults the active script slot.
 *
 * Marks an in-range active slot as faulted and returns the execution fault status.
 *
 * @param[in,out] runtime Script state containing the active slot.
 * @return The execution fault status.
 */
static ForceFeedbackScriptExecutionStatus fault(ForceFeedbackScriptRuntime *runtime) {
    if (runtime->active_slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT) {
        runtime->slots[runtime->active_slot].state = FORCE_FEEDBACK_SCRIPT_SLOT_FAULT;
    }
    return FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT;
}

/**
 * @brief Completes the active script slot.
 *
 * Marks the active slot inactive and returns the explicit completion status.
 *
 * @param[in,out] runtime Script state containing the active slot.
 * @return The explicit completion status.
 * @pre The active slot index is in range.
 */
static ForceFeedbackScriptExecutionStatus complete(ForceFeedbackScriptRuntime *runtime) {
    runtime->slots[runtime->active_slot].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
    return FORCE_FEEDBACK_SCRIPT_EXECUTION_COMPLETED;
}

ForceFeedbackScriptExecutionStatus
force_feedback_script_execute(ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                              size_t length) {
    if (runtime->active_slot >= FORCE_FEEDBACK_SCRIPT_SLOT_COUNT) {
        return FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT;
    }

    ExecutionState state = {.valid = true};
    while (state.cursor < length) {
        uint8_t command = script[state.cursor++];
        if (state.remaining != 0) {
            state.remaining--;
        }
        bool commit = state.remaining == 0;

        if (command == COMMAND_CURSOR) {
            continue;
        }
        if (command == COMMAND_ADVANCE) {
            state = set_advance(runtime, script, length, state);
            if (!state.valid) {
                return fault(runtime);
            }
            continue;
        }
        if (command == COMMAND_ADVANCE_IF_ZERO || command == COMMAND_ADVANCE_IF_NONZERO) {
            bool advance_when_zero = command == COMMAND_ADVANCE_IF_ZERO;
            state = conditional_advance(runtime, script, length, state, advance_when_zero);
            if (!state.valid) {
                return fault(runtime);
            }
            continue;
        }
        if (command == COMMAND_ENSURE_CURSOR) {
            if (commit) {
                return FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED;
            }
            continue;
        }
        if (command == COMMAND_RESERVE_SLOT) {
            if (commit) {
                return complete(runtime);
            }
            continue;
        }
        if (command >= COMMAND_REWIND_IF_ZERO && command <= COMMAND_COMPLETE_IF_NONZERO) {
            ForceFeedbackScriptOperandResult condition =
                force_feedback_script_operand_read(runtime, script, length, state.cursor);
            state.cursor = condition.cursor;
            if (!condition.valid) {
                return fault(runtime);
            }
            bool selected = is_zero(condition.value) == (command == COMMAND_REWIND_IF_ZERO ||
                                                         command == COMMAND_COMPLETE_IF_ZERO);
            if (selected && commit) {
                return command <= COMMAND_REWIND_IF_NONZERO
                           ? FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED
                           : complete(runtime);
            }
            continue;
        }
        if (!force_feedback_script_operation_supported(command)) {
            fault(runtime);
            return FORCE_FEEDBACK_SCRIPT_EXECUTION_SILENT_FAULT;
        }
        ForceFeedbackScriptDestinationResult operation = force_feedback_script_operation_execute(
            runtime, command, script, length, state.cursor, commit);
        state.cursor = operation.cursor;
        if (!operation.valid) {
            return fault(runtime);
        }
    }

    if (state.remaining > 1) {
        return fault(runtime);
    }
    return FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED;
}
