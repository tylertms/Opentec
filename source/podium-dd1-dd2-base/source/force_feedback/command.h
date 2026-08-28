#ifndef OPENTEC_BASE_FORCE_FEEDBACK_COMMAND_H
#define OPENTEC_BASE_FORCE_FEEDBACK_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"

enum {
    FORCE_FEEDBACK_EFFECT_SLOT_COUNT = 16,
    FORCE_FEEDBACK_POSITION_EFFECT_SLOT = 16,
};

typedef enum {
    FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1,
    FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2,
    FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3,
    FORCE_FEEDBACK_COMMAND_CLEAR_EFFECT,
    FORCE_FEEDBACK_COMMAND_ACTIVATE_POSITION_EFFECT,
    FORCE_FEEDBACK_COMMAND_CLEAR_POSITION_EFFECT,
    FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT,
    FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT,
} ForceFeedbackCommandKind;

typedef struct {
    ForceFeedbackCommandKind kind;
    uint8_t slot;
    int32_t magnitude;
    uint8_t positions[2];
    uint8_t axis_modes[2];
    int8_t directions[2];
    uint16_t strength;
    uint8_t mode;
    bool output_disabled;
} ForceFeedbackCommand;

bool force_feedback_command_decode(const UsbOutputCommand *output, ForceFeedbackCommand *command);

#endif
