#include "force_feedback/script_executor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operation.h"

enum {
    COMMAND_CURSOR = 0x00,
    COMMAND_ADVANCE = 0x01,
    COMMAND_ADVANCE_IF_ZERO = 0x02,
    COMMAND_ADVANCE_IF_NONZERO = 0x03,
    COMMAND_STOP = 0x04,
    COMMAND_STOP_IF_ZERO = 0x05,
    COMMAND_STOP_IF_NONZERO = 0x06,
    COMMAND_COMPLETE = 0x07,
    COMMAND_COMPLETE_IF_ZERO = 0x08,
    COMMAND_COMPLETE_IF_NONZERO = 0x09,
};

typedef union {
    float number;
    uint32_t bits;
} ExecutionValue;

typedef struct {
    size_t cursor;
    uint16_t remaining;
    bool valid;
} ExecutionState;

static bool is_zero(uint32_t value) { return (ExecutionValue){.bits = value}.number == 0.0f; }

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

static ForceFeedbackScriptExecutionStatus fault(ForceFeedbackScriptRuntime *runtime) {
    if (runtime->active_slot < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT) {
        runtime->slots[runtime->active_slot].state = FORCE_FEEDBACK_SCRIPT_SLOT_FAULT;
    }
    return FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT;
}

static ForceFeedbackScriptExecutionStatus complete(ForceFeedbackScriptRuntime *runtime) {
    runtime->slots[runtime->active_slot].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
    return FORCE_FEEDBACK_SCRIPT_EXECUTION_COMPLETED;
}

/**
 * @brief Execute a complete force-feedback script byte sequence.
 *
 * Processes records in order, decodes operation operands, applies conditional record suppression,
 * and honors stop and completion commands. Suppressed records are fully decoded and evaluated but
 * do not write their destinations. Invalid commands, operands, operation domains, active slots, or
 * an unfinished suppression span fault the active slot. Dynamic commands 0xF0 through 0xF2 have no
 * initialized firmware handlers and are rejected as invalid commands.
 *
 * @param[in,out] runtime Script state and active slot selected by the caller.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of script bytes.
 * @return The natural-finish, stopped, completed, or fault status.
 * @pre runtime and script point to valid objects.
 */
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
        if (command == COMMAND_STOP) {
            if (commit) {
                return FORCE_FEEDBACK_SCRIPT_EXECUTION_STOPPED;
            }
            continue;
        }
        if (command == COMMAND_COMPLETE) {
            if (commit) {
                return complete(runtime);
            }
            continue;
        }
        if (command >= COMMAND_STOP_IF_ZERO && command <= COMMAND_COMPLETE_IF_NONZERO) {
            ForceFeedbackScriptOperandResult condition =
                force_feedback_script_operand_read(runtime, script, length, state.cursor);
            state.cursor = condition.cursor;
            if (!condition.valid) {
                return fault(runtime);
            }
            bool selected = is_zero(condition.value) == (command == COMMAND_STOP_IF_ZERO ||
                                                         command == COMMAND_COMPLETE_IF_ZERO);
            if (selected && commit) {
                return command <= COMMAND_STOP_IF_NONZERO ? FORCE_FEEDBACK_SCRIPT_EXECUTION_STOPPED
                                                          : complete(runtime);
            }
            continue;
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
