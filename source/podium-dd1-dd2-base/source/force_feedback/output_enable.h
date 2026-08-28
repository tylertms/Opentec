#ifndef OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_ENABLE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_OUTPUT_ENABLE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FORCE_OUTPUT_ENABLE_PROMPT_REQUIRED,
    FORCE_OUTPUT_ENABLE_AWAITING_CONFIRMATION,
} ForceOutputEnablePhase;

typedef enum {
    FORCE_OUTPUT_ENABLE_ACTION_NONE,
    FORCE_OUTPUT_ENABLE_ACTION_SHOW_PROMPT,
    FORCE_OUTPUT_ENABLE_ACTION_CANCEL_PROMPT,
    FORCE_OUTPUT_ENABLE_ACTION_DISMISS_PROMPT,
} ForceOutputEnableAction;

typedef struct {
    uint16_t response;
    ForceOutputEnablePhase phase;
} ForceOutputEnable;

void force_output_enable_set_response(ForceOutputEnable *enable, uint16_t response);
bool force_output_enable_service(ForceOutputEnable *enable, bool wheel_protocol_ready,
                                 bool usb_connected, bool action_queue_available,
                                 ForceOutputEnableAction *action);

#endif
