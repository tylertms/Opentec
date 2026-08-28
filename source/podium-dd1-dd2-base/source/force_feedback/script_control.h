#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_CONTROL_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    FORCE_FEEDBACK_SCRIPT_SLOT_COUNT = 16,
};

typedef uint8_t ForceFeedbackRuntimeMode;

enum {
    FORCE_FEEDBACK_RUNTIME_ACTIVE = 0,
    FORCE_FEEDBACK_RUNTIME_POSITION_ONLY = 1,
    FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT = 2,
};

typedef uint8_t ForceFeedbackScriptSlotState;

enum {
    FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY = 0,
    FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE = 1,
    FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE = 2,
    FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED = 3,
    FORCE_FEEDBACK_SCRIPT_SLOT_SERIALIZED_FAULT = 4,
    FORCE_FEEDBACK_SCRIPT_SLOT_FAULT = UINT8_MAX,
};

typedef uint8_t ForceFeedbackScriptSlotCommand;

enum {
    FORCE_FEEDBACK_SCRIPT_SLOT_CLEAR = 0,
    FORCE_FEEDBACK_SCRIPT_SLOT_START = 1,
    FORCE_FEEDBACK_SCRIPT_SLOT_STOP = 2,
    FORCE_FEEDBACK_SCRIPT_SLOT_PAUSE = 3,
    FORCE_FEEDBACK_SCRIPT_SLOT_RESUME = 4,
};

typedef struct {
    ForceFeedbackScriptSlotCommand slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT];
    ForceFeedbackRuntimeMode runtime_mode;
} ForceFeedbackScriptControl;

typedef struct {
    ForceFeedbackScriptSlotState slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT];
    ForceFeedbackRuntimeMode runtime_mode;
} ForceFeedbackScriptStatus;

typedef struct {
    ForceFeedbackScriptSlotState state;
    bool reset_runtime;
} ForceFeedbackScriptSlotTransition;

typedef struct {
    uint32_t ticks;
    uint32_t slot_ticks[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT];
    uint8_t active_slot;
    bool script_executing;
} ForceFeedbackScriptClock;

bool force_feedback_script_control_decode(const uint8_t *packet, size_t length,
                                          ForceFeedbackScriptControl *control);
bool force_feedback_script_status_encode(const ForceFeedbackScriptStatus *status, uint8_t *response,
                                         size_t length);
bool force_feedback_script_slot_transition(ForceFeedbackScriptSlotState current,
                                           ForceFeedbackScriptSlotCommand command,
                                           ForceFeedbackScriptSlotTransition *transition);
void force_feedback_script_clock_tick(ForceFeedbackScriptClock *clock,
                                      ForceFeedbackRuntimeMode mode);

#endif
