#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_CONTROL_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Capacity of force-feedback script slot state.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_SLOT_COUNT = 16, /**< Number of script slots. */
};

/**
 * @brief Identifies the force-feedback runtime output mode.
 *
 * The mode controls whether the script clock advances and whether normal force, position-only, or
 * zero output is selected by the surrounding runtime.
 */
typedef uint8_t ForceFeedbackRuntimeMode;

/**
 * @brief Force-feedback runtime output modes.
 */
enum {
    FORCE_FEEDBACK_RUNTIME_ACTIVE = 0,        /**< Normal force-feedback runtime mode. */
    FORCE_FEEDBACK_RUNTIME_POSITION_ONLY = 1, /**< Position-only runtime mode. */
    FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT = 2,   /**< Runtime mode with zero force output. */
};

/**
 * @brief Identifies the lifecycle state of one force-feedback script slot.
 */
typedef uint8_t ForceFeedbackScriptSlotState;

/**
 * @brief Force-feedback script slot lifecycle states.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY = 0,    /**< Slot contains no loaded script. */
    FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE = 1,   /**< Slot is eligible for script execution. */
    FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE = 2, /**< Slot is loaded but not executing. */
    FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED = 3,   /**< Slot is paused after being active. */
    FORCE_FEEDBACK_SCRIPT_SLOT_SERIALIZED_FAULT =
        4, /**< Serialized value representing a runtime fault. */
    FORCE_FEEDBACK_SCRIPT_SLOT_FAULT = UINT8_MAX, /**< Runtime execution fault state. */
};

/**
 * @brief Identifies a lifecycle command for one force-feedback script slot.
 */
typedef uint8_t ForceFeedbackScriptSlotCommand;

/**
 * @brief Force-feedback script slot lifecycle commands.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_SLOT_CLEAR = 0,  /**< Empties the slot. */
    FORCE_FEEDBACK_SCRIPT_SLOT_START = 1,  /**< Starts a non-empty slot. */
    FORCE_FEEDBACK_SCRIPT_SLOT_STOP = 2,   /**< Stops a non-empty slot. */
    FORCE_FEEDBACK_SCRIPT_SLOT_PAUSE = 3,  /**< Pauses an active slot. */
    FORCE_FEEDBACK_SCRIPT_SLOT_RESUME = 4, /**< Resumes a paused slot. */
};

/**
 * @brief Decoded force-feedback script control command.
 *
 * The command contains one four-bit lifecycle command for each script slot and one runtime mode.
 */
typedef struct {
    ForceFeedbackScriptSlotCommand
        slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT]; /**< Per-slot commands. */
    ForceFeedbackRuntimeMode runtime_mode;       /**< Requested force-feedback runtime mode. */
} ForceFeedbackScriptControl;

/**
 * @brief Result of decoding a force-feedback script control packet.
 *
 * The value is meaningful only when valid is true.
 */
typedef struct {
    ForceFeedbackScriptControl value; /**< Decoded control fields. */
    bool valid;                       /**< Whether the packet was complete and had the opcode. */
} ForceFeedbackScriptControlResult;

/**
 * @brief Runtime state and metrics for one force-feedback script slot.
 *
 * The four values are script-visible storage; the remaining counters record execution timing and
 * count information maintained by the script service.
 */
typedef struct {
    ForceFeedbackScriptSlotState state; /**< Current slot lifecycle state. */
    uint32_t values[4];                 /**< Four raw script-visible slot values. */
    uint32_t average_rate;              /**< Raw bits of the average execution rate. */
    uint32_t delta_rate;      /**< Raw bits of elapsed time in seconds since the snapshot. */
    uint32_t execution_count; /**< Number of executions recorded for the slot. */
    uint32_t tick_snapshot;   /**< Slot-clock value captured before execution. */
} ForceFeedbackScriptSlot;

/**
 * @brief Clock counters used by the force-feedback script runtime.
 *
 * The service and timer path share the counters. The active slot and execution flag identify which
 * slot receives slot-clock ticks while script execution is in progress.
 */
typedef struct {
    volatile uint32_t ticks; /**< Engine clock ticks for active or zero-output mode. */
    volatile uint32_t slot_ticks[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT]; /**< Per-slot clock ticks. */
    volatile uint32_t motion_ticks; /**< Clock ticks used by motion processing. */
    volatile uint8_t active_slot;   /**< Slot index charged while execution is active. */
    volatile bool script_executing; /**< Whether a script currently receives slot ticks. */
} ForceFeedbackScriptClock;

/**
 * @brief Decode a force-feedback script control packet.
 *
 * Requires opcode 0x0c and at least 13 bytes. Bytes 4 through 11 contain two four-bit slot
 * commands each, and byte 12 contains the runtime mode. The decoder does not validate command or
 * mode values; invalid packet pointers, lengths, or opcodes return an invalid result.
 *
 * @param[in] packet Packet beginning with the script-control opcode.
 * @param[in] length Number of bytes available in packet.
 * @return Decoded control fields and a valid flag indicating packet acceptance.
 */
ForceFeedbackScriptControlResult force_feedback_script_control_decode(const uint8_t *packet,
                                                                      size_t length);

/**
 * @brief Apply one lifecycle command to a force-feedback script slot.
 *
 * Clear sets the state to empty without changing payload fields. Start accepts any non-empty slot,
 * clears its four values and four metrics, and activates it; stop accepts any non-empty slot and
 * makes it inactive. Pause and resume require active and paused states respectively; null slots and
 * rejected commands are unchanged.
 *
 * @param[in,out] slot Slot state to update.
 * @param[in] command Lifecycle command to apply.
 * @return true when the non-null slot accepts the command; otherwise false.
 */
bool force_feedback_script_slot_apply(ForceFeedbackScriptSlot *slot,
                                      ForceFeedbackScriptSlotCommand command);

/**
 * @brief Advance force-feedback script runtime clocks by one tick.
 *
 * The engine clock advances in active and zero-output modes, the active slot clock advances while
 * script execution is flagged with an in-range slot, and the motion clock advances in every mode.
 * A null clock is ignored.
 *
 * @param[in,out] clock Runtime clock counters to advance.
 * @param[in] mode Current force-feedback runtime mode.
 */
void force_feedback_script_clock_tick(ForceFeedbackScriptClock *clock,
                                      ForceFeedbackRuntimeMode mode);

#endif
