#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RUNTIME_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_motion.h"
#include "force_feedback/script_operand.h"
#include "force_feedback/script_scheduler.h"
#include "force_feedback/script_store.h"

/**
 * @brief Complete runtime state for the force-feedback script engine.
 *
 * The system groups script values, uploaded storage, host inputs, timing, motion history,
 * scheduling, and the selected runtime mode.
 */
typedef struct {
    ForceFeedbackScriptRuntime values;      /**< Script-visible values and slot state. */
    ForceFeedbackScriptStore store;         /**< Uploaded script storage and allocation records. */
    ForceFeedbackScriptInputs inputs;       /**< Current host input and live-input slots. */
    ForceFeedbackScriptClock clock;         /**< Engine, slot, and motion clock state. */
    ForceFeedbackScriptMotionState motion;  /**< Previous state for derived motion values. */
    ForceFeedbackScriptScheduler scheduler; /**< Deadline state for scheduled script ticks. */
    uint32_t host_tick_snapshot;            /**< Engine-clock snapshot for the latest host tick. */
    uint32_t idle_tick_snapshot;            /**< Engine-clock snapshot for the latest idle tick. */
    volatile ForceFeedbackRuntimeMode mode; /**< Current script runtime mode. */
} ForceFeedbackScriptSystem;

/**
 * @brief Initialize the complete force-feedback script runtime for cold startup.
 *
 * Initializes runtime values, marks every sample entry unused, and resets storage, input, timing,
 * motion, and scheduler state, then selects position-only mode and requests an initial position
 * output. A null system pointer is ignored.
 *
 * @param[out] system Script system to initialize.
 */
void force_feedback_script_runtime_init(ForceFeedbackScriptSystem *system);

/**
 * @brief Reset the force-feedback script session state.
 *
 * Matches the official session reset by clearing script-visible values, slot values and metrics,
 * motion values, axes, rotation range, input state, sample values, uploaded script storage, engine
 * ticks, active-slot selection, and the pending position request. Motion history, snapshots,
 * deadlines, and motion ticks are retained. A null system pointer is ignored.
 *
 * @param[in,out] system Script system whose session-owned state is reset.
 */
void force_feedback_script_runtime_reset(ForceFeedbackScriptSystem *system);

/**
 * @brief Apply one host packet to the force-feedback script system.
 *
 * Routes sample (0x0b), control (0x0c), upload (0x0d), and input (0x0e) packets to their owning
 * runtime components and updates the retained position when an active or ready input packet
 * provides a position slot. Unknown opcodes and malformed packets leave the system unchanged.
 *
 * @param[in,out] system Script system to update.
 * @param[in] packet Host packet bytes beginning with a script opcode.
 * @param[in] length Number of available packet bytes.
 * @return true when the packet is supported and valid; otherwise false.
 */
bool force_feedback_script_runtime_apply_packet(ForceFeedbackScriptSystem *system,
                                                const uint8_t *packet, size_t length);

/**
 * @brief Apply one script-control packet to the runtime.
 *
 * Decodes and applies the sixteen packed slot lifecycle commands, ignores commands rejected by
 * their current slot states, updates the runtime mode from the packet, and compacts storage
 * released by cleared slots.
 *
 * @param[in,out] system Script system whose slots, mode, and storage are updated.
 * @param[in] packet Script-control packet.
 * @param[in] length Number of available packet bytes.
 * @return true when the complete control packet is valid and applied; otherwise false.
 */
bool force_feedback_script_runtime_apply_control(ForceFeedbackScriptSystem *system,
                                                 const uint8_t *packet, size_t length);

#endif
