#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_ENABLE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_ENABLE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Phase of the high-torque output confirmation sequence.
 *
 * The phase records whether a prompt is required or the service is waiting for the user's response
 * before releasing the output interlock.
 */
typedef enum {
    FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED, /**< No prompt is active and a new prompt is required. */
    FORCE_OUTPUT_ENABLE_AWAITING_CONFIRMATION, /**< A prompt is active and awaiting confirmation. */
} ForceOutputEnablePhase;

/**
 * @brief Display action emitted by the output-enable confirmation service.
 *
 * Actions are requests for the display queue; the service emits at most one action per call.
 */
typedef enum {
    FORCE_OUTPUT_ENABLE_ACTION_NONE,          /**< No display action is requested. */
    FORCE_OUTPUT_ENABLE_ACTION_SHOW_PROMPT,   /**< Request display of the high-torque output prompt.
                                               */
    FORCE_OUTPUT_ENABLE_ACTION_CANCEL_PROMPT, /**< Request cancellation of an active high-torque
                                                 output prompt. */
    FORCE_OUTPUT_ENABLE_ACTION_DISMISS_PROMPT, /**< Request dismissal of a confirmed high-torque
                                                  output prompt. */
} ForceOutputEnableAction;

/**
 * @brief Persistent state for high-torque output confirmation.
 *
 * The phase controls prompt and interlock transitions between service calls, while the response
 * stores the latest value supplied by the display interaction.
 */
typedef struct {
    uint16_t response; /**< Pending response value supplied by the display interaction. */
    ForceOutputEnablePhase phase; /**< Current prompt and confirmation phase. */
} ForceOutputEnable;

/**
 * @brief Stores a response for the high-torque output prompt.
 *
 * Replaces the pending response value that the output-enable service checks on its next call.
 *
 * @param[in,out] enable Output-enable confirmation state.
 * @param[in] response Response value supplied by the display interaction.
 */
void force_output_enable_set_response(ForceOutputEnable *enable, uint16_t response);

/**
 * @brief Advances the high-torque output confirmation sequence.
 *
 * Requests a prompt only when protocol, USB, and action-queue prerequisites are ready. An active
 * prompt is cancelled when a prerequisite is lost and cancellation can be queued; output is
 * released only for response one.
 *
 * @param[in,out] enable Output-enable confirmation state to advance.
 * @param[in] wheel_protocol_ready Whether wheel protocol selection is complete.
 * @param[in] usb_connected Whether a USB host connection is present.
 * @param[in] action_queue_available Whether a display action can be queued this pass.
 * @param[out] action Display action requested during this service pass.
 * @return true while force output remains interlocked; otherwise false.
 */
bool force_output_enable_service(ForceOutputEnable *enable, bool wheel_protocol_ready,
                                 bool usb_connected, bool action_queue_available,
                                 ForceOutputEnableAction *action);

#endif
