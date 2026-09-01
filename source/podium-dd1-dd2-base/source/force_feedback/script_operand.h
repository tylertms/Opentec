#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERAND_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_control.h"
#include "force_feedback/script_input.h"

/**
 * @brief Sizes and indexes used by script operands.
 *
 * The values describe the fixed script variable, motion, and axis banks exposed by the runtime.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT = 12,         /**< Number of script variables. */
    FORCE_FEEDBACK_SCRIPT_WRITABLE_VARIABLE_COUNT = 8, /**< Number of writable script variables. */
    FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT_VARIABLE =
        10, /**< Variable index containing the sample count. */
    FORCE_FEEDBACK_SCRIPT_MOTION_VALUE_COUNT = 8, /**< Number of script motion values. */
    FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT = 10,  /**< Number of script axis values. */
};

/**
 * @brief Runtime values and state exposed to script operands.
 *
 * Raw 32-bit script values are stored for variables, motion values, axes, samples, and slot
 * metrics, while the range fields and active slot select context for indirect operands.
 */
typedef struct {
    uint32_t variables[FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT];  /**< Script variable values. */
    uint32_t motion[FORCE_FEEDBACK_SCRIPT_MOTION_VALUE_COUNT]; /**< Script motion values. */
    uint32_t axes[FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT];     /**< Script axis values. */
    ForceFeedbackScriptSamples samples;                        /**< Script sample table. */
    ForceFeedbackScriptSlot
        slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT]; /**< Runtime state for each script slot. */
    uint16_t extended_rotation_range; /**< Extended rotation range used by range code 126 or 127. */
    uint8_t rotation_range_code;      /**< Encoded active rotation range. */
    uint8_t active_slot;              /**< Script slot currently selected for execution. */
} ForceFeedbackScriptRuntime;

/**
 * @brief Result of decoding one script operand.
 *
 * The result carries the raw value and the cursor after the encoded operand, or the cursor reached
 * when decoding fails.
 */
typedef struct {
    uint32_t value; /**< Resolved raw operand value. */
    size_t cursor;  /**< Offset following the operand or at the failure point. */
    bool valid;     /**< Whether a complete recognized operand was decoded. */
} ForceFeedbackScriptOperandResult;

/**
 * @brief Result of consuming one script destination.
 *
 * The result carries the cursor after the encoded destination and whether destination processing
 * accepted the available encoding for the requested commit mode.
 */
typedef struct {
    size_t cursor; /**< Offset following the destination or at the failure point. */
    bool valid; /**< Whether destination bytes were consumed; with commit, whether the destination
                   was accepted. */
} ForceFeedbackScriptDestinationResult;

/**
 * @brief Decode one encoded script operand.
 *
 * Resolves built-in constants, big-endian immediates, scaled percentage and per-mille literals,
 * samples, variables, active-slot values and metrics, motion values, and axes without changing
 * runtime state. The returned cursor follows a complete operand and reaches the available length
 * for an incomplete operand.
 *
 * @param[in] runtime Runtime state referenced by indirect operands.
 * @param[in] script Encoded script bytes.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the operand.
 * @return The resolved value and following cursor when valid; otherwise an invalid result.
 * @pre runtime and script point to valid objects.
 */
ForceFeedbackScriptOperandResult
force_feedback_script_operand_read(const ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                                   size_t length, size_t cursor);

/**
 * @brief Consume one encoded script destination and optionally write a raw value.
 *
 * Directly writes supported sample, variable, slot, motion, or axis destinations when commit is
 * true. When commit is false, consumes the destination encoding without changing runtime state.
 *
 * @param[in,out] runtime Runtime state selected by the destination.
 * @param[in] script Encoded script bytes.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the destination.
 * @param[in] value Raw 32-bit value to write when commit is true.
 * @param[in] commit true to update runtime state; false to consume without writing.
 * @return The following cursor and acceptance state; valid is false when required bytes are absent
 * or a committed destination is unsupported.
 * @pre runtime and script point to valid objects.
 */
ForceFeedbackScriptDestinationResult
force_feedback_script_operand_write(ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                                    size_t length, size_t cursor, uint32_t value, bool commit);

#endif
