#ifndef OPENTEC_BASE_SYSTEM_TORQUE_KEY_PROMPT_H
#define OPENTEC_BASE_SYSTEM_TORQUE_KEY_PROMPT_H

#include <stdbool.h>

/**
 * @brief Torque Key calibration-display phases.
 *
 * Numeric values match the official calibration display service at base-binary addresses
 * 0x046498-0x046628.
 */
typedef enum {
    TORQUE_KEY_PROMPT_IDLE = 0,
    TORQUE_KEY_PROMPT_SHOW_REQUIRED = 1,
    TORQUE_KEY_PROMPT_AWAITING_CONFIRMATION = 2,
    TORQUE_KEY_PROMPT_TORQUE_ENABLED = 3,
    TORQUE_KEY_PROMPT_REDUCED_TORQUE = 4,
    TORQUE_KEY_PROMPT_DISMISS_REQUIRED = 5,
    TORQUE_KEY_PROMPT_SHOW_REDUCED_REQUIRED = 6,
} TorqueKeyPromptPhase;

/**
 * @brief Stable input values used by the Torque Key calibration-display service.
 *
 * Numeric values match the official input-state byte at base-binary addresses 0x046240-0x046274.
 */
typedef enum {
    TORQUE_KEY_INPUT_UNKNOWN = 0,
    TORQUE_KEY_INPUT_LOW = 1,
    TORQUE_KEY_INPUT_HIGH = 2,
} TorqueKeyInputState;

/**
 * @brief Action requested by the Torque Key calibration-display service.
 *
 * Actions carrying a local display command must be acknowledged with
 * torque_key_prompt_accept_action() only after that command is accepted. Native display state
 * changes therefore cannot advance the phase while their local command is blocked.
 */
typedef enum {
    TORQUE_KEY_PROMPT_ACTION_NONE,
    TORQUE_KEY_PROMPT_ACTION_SHOW_PROMPT,
    TORQUE_KEY_PROMPT_ACTION_DISMISS_TORQUE_KEY_PROMPT,
    TORQUE_KEY_PROMPT_ACTION_ENABLE_TORQUE,
    TORQUE_KEY_PROMPT_ACTION_DISMISS_CURRENT,
    TORQUE_KEY_PROMPT_ACTION_DISMISS_REDUCED_TORQUE,
    TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_QUICK_RELEASE,
    TORQUE_KEY_PROMPT_ACTION_SHOW_REDUCED_STEERING_WHEEL,
} TorqueKeyPromptAction;

/**
 * @brief Torque Key calibration-display policy state.
 */
typedef struct {
    TorqueKeyPromptPhase phase; /**< Current official calibration-display phase. */
    bool response_pending; /**< Accepted local-display response awaiting service. */
} TorqueKeyPrompt;

/**
 * @brief Initializes Torque Key calibration-display policy.
 *
 * Starts at official phase zero with no accepted response.
 *
 * @param[out] prompt Torque Key policy to initialize.
 */
void torque_key_prompt_init(TorqueKeyPrompt *prompt);

/**
 * @brief Stores an accepted Torque Key response.
 *
 * Only response one while phase two is active is retained, matching the official response branch.
 *
 * @param[in,out] prompt Torque Key policy state.
 * @param[in] accepted True when the local display accepted the safety prompt.
 */
void torque_key_prompt_set_response(TorqueKeyPrompt *prompt, bool accepted);

/**
 * @brief Selects the next Torque Key calibration-display action.
 *
 * Applies official revocation before dispatching the current phase. Button scanning or attached
 * wheel calibration moves phases two and three to phase five. The returned action does not commit
 * a command-backed phase change until torque_key_prompt_accept_action() is called.
 *
 * @param[in,out] prompt Torque Key policy state.
 * @param[in] input Stable active-low Torque Key input state.
 * @param[in] button_scan_pending True while the wheel button scan is active.
 * @param[in] calibration_available True while wheel calibration controls are available.
 * @param[in] protocol_request_pending True after a display or alternate wheel report was received.
 * @return One action for the current service pass, or NONE.
 */
TorqueKeyPromptAction torque_key_prompt_service(TorqueKeyPrompt *prompt,
                                                TorqueKeyInputState input,
                                                bool button_scan_pending,
                                                bool calibration_available,
                                                bool protocol_request_pending);

/**
 * @brief Commits an accepted Torque Key calibration-display action.
 *
 * The caller invokes this only after the action's local display command has entered its queue.
 * Repeated or stale actions are ignored.
 *
 * @param[in,out] prompt Torque Key policy state.
 * @param[in] action Action accepted by the display/event queue.
 */
void torque_key_prompt_accept_action(TorqueKeyPrompt *prompt, TorqueKeyPromptAction action);

#endif
