#ifndef OPENTEC_BASE_BOARD_POWER_H
#define OPENTEC_BASE_BOARD_POWER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief States of the wheel-base physical profile-save controller.
 *
 * The controller follows the RD9 request, unlock, hold, and completion phases while retaining its
 * current deadline and torque-disable request.
 */
typedef enum {
    POWER_PHASE_WAIT_FOR_REQUEST, /**< Waiting for the first active RD9 sample. */
    POWER_PHASE_WAIT_FOR_UNLOCK,  /**< Waiting for security unlock while RD9 remains active. */
    POWER_PHASE_WAIT_FOR_HOLD,    /**< Waiting for the qualifying RD9 hold to complete. */
    POWER_PHASE_COMPLETE,         /**< Official profile-save state 3; finalization is active. */
} PowerPhase;

/**
 * @brief Actions emitted by the physical profile-save controller.
 *
 * Each update returns one action for the caller to apply to the RD8 latch, torque request, or
 * profile-save sequence.
 */
typedef enum {
    POWER_ACTION_NONE,                   /**< No profile-save transition occurred. */
    POWER_ACTION_ENABLE_LATCH,           /**< Enable the hardware power latch. */
    POWER_ACTION_TORQUE_REQUEST_CHANGED, /**< The short-press torque request changed. */
    POWER_ACTION_BEGIN_PROFILE_SAVE,     /**< Begin the profile-save sequence. */
    POWER_ACTION_FINISH_PROFILE_SAVE,    /**< Complete the profile-save sequence. */
} PowerAction;

/**
 * @brief Stateful physical profile-save controller.
 *
 * Stores the current phase, the active modular deadline, and the requested torque-disable state
 * used across periodic updates.
 */
typedef struct {
    PowerPhase phase;                /**< Current physical profile-save phase. */
    uint32_t deadline_ms;            /**< Active hold deadline in milliseconds. */
    uint32_t completion_deadline_ms; /**< Display-finalization deadline in milliseconds. */
    bool torque_disabled;            /**< True when a short press requested torque disable. */
} PowerController;

/**
 * @brief Initializes the wheel-base profile-save controller.
 *
 * Clears the controller into POWER_PHASE_WAIT_FOR_REQUEST with no active deadline or torque
 * disable request.
 *
 * @param[out] controller Power state to initialize.
 */
void power_controller_init(PowerController *controller);

/**
 * @brief Reports whether physical profile-save completion is active.
 *
 * The official physical profile-save state 3 is represented by POWER_PHASE_COMPLETE in the
 * platform-neutral power lifecycle.
 *
 * @param[in] controller Power lifecycle to inspect.
 * @return true while the physical profile-save completion state is active; otherwise false.
 */
bool power_controller_profile_save_complete(const PowerController *controller);

/**
 * @brief Arms the state-three finalization deadline.
 *
 * Reanchors the official one-second interval after synchronous profile-save side effects have
 * finished. Calls outside POWER_PHASE_COMPLETE are ignored.
 *
 * @param[in,out] controller Profile-save lifecycle to update.
 * @param[in] now_ms Current monotonic time after profile-save side effects.
 */
void power_controller_arm_profile_save_completion(PowerController *controller, uint32_t now_ms);

/**
 * @brief Advances physical profile-save behavior.
 *
 * Enables the RD8 hold latch on the first RD9 sample, toggles torque disable on qualifying short
 * releases, and emits profile-save actions only after the strict hold deadline passes. State-three
 * finalization is emitted on every service pass so the caller can apply its presentation deadline
 * independently from unconditional hardware actions.
 *
 * @param[in,out] controller Persistent power phase, deadline, and torque-disable state.
 * @param[in] profile_save_input_active True while RD9 is asserted.
 * @param[in] profile_save_control_enabled True when the retained security gate permits an unlock
 * phase to begin short-press or hold handling.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return The single transition action produced by this update.
 */
PowerAction power_controller_update(PowerController *controller, bool profile_save_input_active,
                                    bool profile_save_control_enabled, uint32_t now_ms);

#endif
