#include "force_feedback/output_enable.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    FORCE_OUTPUT_ENABLE_ACCEPTED = 1,
};

/**
 * @brief Stores the response to the high-torque output prompt.
 *
 * Replaces the pending response consumed by the output-enable service.
 *
 * @param[in,out] enable Output-enable confirmation state.
 * @param[in] response Prompt response value supplied by the display interaction.
 */
void force_output_enable_set_response(ForceOutputEnable *enable, uint16_t response) {
    enable->response = response;
}

/**
 * @brief Services the high-torque output confirmation sequence.
 *
 * Waits until the wheel protocol and USB connection are ready, requests the enable prompt when the
 * action queue accepts it, and releases output only for response value one. A lost prerequisite
 * cancels an active prompt after the cancellation action can be queued.
 *
 * @param[in,out] enable Output-enable confirmation state.
 * @param[in] wheel_protocol_ready True after attached-wheel mode selection has completed.
 * @param[in] usb_connected True while USB connection sensing reports an attached host.
 * @param[in] action_queue_available True when a new display action can be accepted.
 * @param[out] action Display action requested during this service pass.
 * @return True while force output must remain interlocked; otherwise false.
 */
bool force_output_enable_service(ForceOutputEnable *enable, bool wheel_protocol_ready,
                                 bool usb_connected, bool action_queue_available,
                                 ForceOutputEnableAction *action) {
    *action = FORCE_OUTPUT_ENABLE_ACTION_NONE;

    switch (enable->phase) {
    case FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED:
        if (!wheel_protocol_ready || !usb_connected || !action_queue_available) {
            return true;
        }
        *action = FORCE_OUTPUT_ENABLE_ACTION_SHOW_PROMPT;
        enable->phase = FORCE_OUTPUT_ENABLE_AWAITING_CONFIRMATION;
        return true;

    case FORCE_OUTPUT_ENABLE_AWAITING_CONFIRMATION:
        if (!wheel_protocol_ready || !usb_connected) {
            if (action_queue_available) {
                *action = FORCE_OUTPUT_ENABLE_ACTION_CANCEL_PROMPT;
                enable->phase = FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED;
            }
            return true;
        }
        if (enable->response != FORCE_OUTPUT_ENABLE_ACCEPTED) {
            return true;
        }
        enable->response = 0;
        enable->phase = FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED;
        *action = FORCE_OUTPUT_ENABLE_ACTION_DISMISS_PROMPT;
        return false;
    }

    return true;
}
