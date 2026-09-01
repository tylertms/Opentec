#ifndef OPENTEC_BASE_SYSTEM_TORQUE_KEY_PROMPT_H
#define OPENTEC_BASE_SYSTEM_TORQUE_KEY_PROMPT_H

#include <stdbool.h>

/**
 * @brief Torque Key acknowledgement-prompt phase.
 *
 * The phase tracks stable key presence, event-queue presentation, operator confirmation, and
 * removal cancellation.
 */
typedef enum {
    TORQUE_KEY_PROMPT_REMOVED,               /**< Key is absent and no prompt is pending. */
    TORQUE_KEY_PROMPT_SHOW_REQUIRED,         /**< Key insertion requires a prompt event. */
    TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION, /**< Prompt is shown and awaits acceptance. */
    TORQUE_KEY_PROMPT_CANCEL_REQUIRED,       /**< Key removal requires prompt cancellation. */
    TORQUE_KEY_PROMPT_ACKNOWLEDGED,          /**< Inserted key has been accepted by the operator. */
} TorqueKeyPromptPhase;

/**
 * @brief Action emitted by the Torque Key prompt controller.
 *
 * The firmware integration layer translates these actions into system event or display updates.
 */
typedef enum {
    TORQUE_KEY_PROMPT_ACTION_NONE,    /**< No prompt action is ready. */
    TORQUE_KEY_PROMPT_ACTION_SHOW,    /**< Queue the prompt display event. */
    TORQUE_KEY_PROMPT_ACTION_CANCEL,  /**< Queue prompt cancellation. */
    TORQUE_KEY_PROMPT_ACTION_DISMISS, /**< Dismiss an acknowledged prompt. */
} TorqueKeyPromptAction;

/**
 * @brief Torque Key prompt policy state.
 *
 * Stores the current prompt phase and an accepted response waiting for service.
 */
typedef struct {
    TorqueKeyPromptPhase phase; /**< Current prompt phase. */
    bool response_pending;      /**< Whether an accepted response awaits service. */
} TorqueKeyPrompt;

/**
 * @brief Initializes Torque Key acknowledgement policy.
 *
 * Starts in the removed phase without a pending operator response.
 *
 * @param[out] prompt Torque Key prompt policy to initialize.
 */
void torque_key_prompt_init(TorqueKeyPrompt *prompt);

/**
 * @brief Applies a stable Torque Key presence change.
 *
 * Insertion from the removed phase requests acknowledgement, while removal cancels an active
 * prompt or returns the policy to its removed phase.
 *
 * @param[in,out] prompt Torque Key prompt policy.
 * @param[in] inserted True when the Torque Key is stably inserted.
 */
void torque_key_prompt_set_inserted(TorqueKeyPrompt *prompt, bool inserted);

/**
 * @brief Stores an accepted Torque Key safety response.
 *
 * Retains an accepted response only while the prompt awaits confirmation; other responses do not
 * advance the policy.
 *
 * @param[in,out] prompt Torque Key prompt policy.
 * @param[in] accepted True for the accepted response value.
 */
void torque_key_prompt_set_response(TorqueKeyPrompt *prompt, bool accepted);

/**
 * @brief Advances Torque Key prompt presentation through the shared event queue.
 *
 * Waits for an available event queue position before showing or cancelling the prompt and dismisses
 * an accepted prompt immediately.
 *
 * @param[in,out] prompt Torque Key prompt policy.
 * @param[in] event_slot_available True when a presentation event can be accepted.
 * @return Prompt presentation action for the firmware integration layer.
 */
TorqueKeyPromptAction torque_key_prompt_service(TorqueKeyPrompt *prompt, bool event_slot_available);

#endif
