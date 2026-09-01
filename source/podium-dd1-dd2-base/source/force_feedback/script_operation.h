#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERATION_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

/**
 * @brief Execute one encoded force-feedback script operation.
 *
 * Decodes the operation's source operands, dispatches arithmetic, logic, comparison, bit, sample,
 * range, or integer work to the corresponding evaluator, and consumes or writes the encoded
 * destination according to commit. Set-bit and clear-bit operations write through their first
 * source operand. When commit is false, operations still decode and evaluate their operands without
 * changing runtime state.
 *
 * @param[in,out] runtime Script values and state referenced by the operation.
 * @param[in] operation Operation byte preceding the operand sequence.
 * @param[in] script Encoded script bytes.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first source operand.
 * @param[in] commit true to commit a supported destination write; false to consume without writing.
 * @return The following cursor and validity state; valid is false when source decoding, operation
 * evaluation, or a committed destination fails.
 * @pre runtime and script point to valid objects.
 */
ForceFeedbackScriptDestinationResult
force_feedback_script_operation_execute(ForceFeedbackScriptRuntime *runtime, uint8_t operation,
                                        const uint8_t *script, size_t length, size_t cursor,
                                        bool commit);

#endif
