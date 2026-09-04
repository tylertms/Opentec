#include "board/power.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Physical profile-save timing constants.
 *
 * This interval defines the strict hold threshold used by the RD9 profile-save service.
 */
enum {
    PROFILE_SAVE_HOLD_MS = 1000, /**< Strict RD9 hold interval in milliseconds. */
    PROFILE_SAVE_COMPLETE_DELAY_MS =
        1000, /**< Strict state-three display interval in milliseconds. */
};

/**
 * @brief Tests whether a strict millisecond deadline has elapsed.
 *
 * Uses signed modular subtraction so both button intervals remain correct when the monotonic
 * counter wraps.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to compare against.
 * @return True only after the deadline, not at the deadline itself.
 */
static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * @brief Initializes the wheel-base profile-save controller.
 *
 * Clears the state and waits for an RD9 sample before requesting the RD8 hold latch; later samples
 * then select short-press or profile-save behavior.
 *
 * @param[out] controller Power state to initialize.
 */
void power_controller_init(PowerController *controller) { *controller = (PowerController){0}; }

/**
 * @brief Reports whether physical profile-save completion is active.
 *
 * Maps the platform-neutral completion phase to the official physical profile-save state 3.
 *
 * @param[in] controller Power lifecycle to inspect.
 * @return true while completion side effects are active; otherwise false.
 */
bool power_controller_profile_save_complete(const PowerController *controller) {
    return controller != 0 && controller->phase == POWER_PHASE_COMPLETE;
}

/**
 * @brief Arms the state-three finalization deadline.
 *
 * Reanchors the official one-second interval after synchronous profile-save side effects have
 * finished.
 *
 * @param[in,out] controller Profile-save lifecycle to update.
 * @param[in] now_ms Current monotonic time after profile-save side effects.
 */
void power_controller_arm_profile_save_completion(PowerController *controller, uint32_t now_ms) {
    if (controller == 0 || controller->phase != POWER_PHASE_COMPLETE) {
        return;
    }
    controller->completion_deadline_ms = now_ms + PROFILE_SAVE_COMPLETE_DELAY_MS;
}

/**
 * @brief Advances physical profile-save behavior.
 *
 * Enables the RD8 hold latch on the first active RD9 sample. When profile-save control is enabled,
 * an active input while unlocked starts the hold deadline; release is handled before checking that
 * strict deadline and toggles the torque-disable request, while a continuously active input starts
 * profile save after the deadline and emits finalization on every state-three service pass. The
 * caller applies the one-second presentation deadline while still performing the unconditional
 * state-three hardware actions.
 *
 * @param[in,out] controller Persistent power phase, deadline, and torque disable request.
 * @param[in] profile_save_input_active True while RD9 is asserted.
 * @param[in] profile_save_control_enabled True when the retained security gate permits the unlock
 * phase to begin short-press or hold handling.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return The single transition action produced by this update.
 */
PowerAction power_controller_update(PowerController *controller, bool profile_save_input_active,
                                    bool profile_save_control_enabled, uint32_t now_ms) {
    switch (controller->phase) {
    case POWER_PHASE_WAIT_FOR_REQUEST:
        if (profile_save_input_active) {
            controller->phase = POWER_PHASE_WAIT_FOR_UNLOCK;
            return POWER_ACTION_ENABLE_LATCH;
        }
        break;
    case POWER_PHASE_WAIT_FOR_UNLOCK:
        if (profile_save_control_enabled && profile_save_input_active) {
            controller->deadline_ms = now_ms + PROFILE_SAVE_HOLD_MS;
            controller->phase = POWER_PHASE_WAIT_FOR_HOLD;
        }
        break;
    case POWER_PHASE_WAIT_FOR_HOLD:
        if (!profile_save_input_active) {
            controller->torque_disabled = !controller->torque_disabled;
            controller->phase = POWER_PHASE_WAIT_FOR_UNLOCK;
            return POWER_ACTION_TORQUE_REQUEST_CHANGED;
        }
        if (deadline_passed(now_ms, controller->deadline_ms)) {
            controller->torque_disabled = false;
            controller->phase = POWER_PHASE_COMPLETE;
            power_controller_arm_profile_save_completion(controller, now_ms);
            return POWER_ACTION_BEGIN_PROFILE_SAVE;
        }
        break;
    case POWER_PHASE_COMPLETE:
        return POWER_ACTION_FINISH_PROFILE_SAVE;
    }
    return POWER_ACTION_NONE;
}
