#include "force_feedback/output_enable.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Response value used by the output-enable confirmation service. */
enum {
    FORCE_OUTPUT_ENABLE_ACCEPTED = 1, /**< Response that confirms output may be enabled. */
};

void force_output_enable_set_response(ForceOutputEnable *enable, uint16_t response) {
    enable->response = response;
}

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
