#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_EXECUTOR_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

/**
 * @brief Identifies the outcome of one force-feedback script execution.
 *
 * The status distinguishes natural end of input, explicit stop, explicit completion, and a
 * malformed or otherwise rejected script.
 */
typedef uint8_t ForceFeedbackScriptExecutionStatus;

/**
 * @brief Force-feedback script execution status values.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED = 0,  /**< Script ended at its input length. */
    FORCE_FEEDBACK_SCRIPT_EXECUTION_STOPPED = 1,   /**< Script executed a stop command. */
    FORCE_FEEDBACK_SCRIPT_EXECUTION_COMPLETED = 2, /**< Script executed a completion command. */
    FORCE_FEEDBACK_SCRIPT_EXECUTION_SILENT_FAULT =
        0xfe, /**< Unsupported command fault without an official status-report request. */
    FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT = UINT8_MAX, /**< Script input or state was rejected. */
};

/**
 * @brief Execute one encoded force-feedback script.
 *
 * Processes records in order, resolves operation operands, applies advance-based suppression, and
 * honors cursor, rewind, reserve, and completion commands. Suppressed operation records are
 * consumed and evaluated with writes disabled. Invalid operands or operation domains return a fault
 * status; an unsupported command returns a silent fault unless more than one suppression count
 * remains, in which case it returns the status that propagates the standard fault report. Errors
 * for an in-range active slot mark that slot faulted. An out-of-range active-slot index and an end
 * of input with more than one suppression count pending also return a fault status.
 *
 * @param[in,out] runtime Script values, samples, outputs, axes, and active-slot state.
 * @param[in] script Encoded script bytes.
 * @param[in] length Number of bytes available in script.
 * @return The finished, stopped, completed, or fault execution status.
 * @pre runtime points to a valid runtime object; when length is nonzero, script points to at least
 * length bytes.
 */
ForceFeedbackScriptExecutionStatus
force_feedback_script_execute(ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                              size_t length);

#endif
