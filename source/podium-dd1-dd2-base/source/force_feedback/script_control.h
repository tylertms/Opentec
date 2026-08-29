#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_CONTROL_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    FORCE_FEEDBACK_SCRIPT_SLOT_COUNT = 16,
    FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE = 22,
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
    ForceFeedbackScriptControl value;
    bool valid;
} ForceFeedbackScriptControlResult;

typedef struct {
    ForceFeedbackScriptSlotState state;
    uint32_t values[4];
    uint32_t average_rate;
    uint32_t delta_rate;
    uint32_t execution_count;
    uint32_t tick_snapshot;
} ForceFeedbackScriptSlot;

typedef struct {
    volatile uint32_t ticks;
    volatile uint32_t slot_ticks[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT];
    volatile uint32_t motion_ticks;
    volatile uint8_t active_slot;
    volatile bool script_executing;
} ForceFeedbackScriptClock;

ForceFeedbackScriptControlResult force_feedback_script_control_decode(const uint8_t *packet,
                                                                      size_t length);
bool force_feedback_script_status_encode(const ForceFeedbackScriptSlot *slots,
                                         ForceFeedbackRuntimeMode mode, uint8_t *sequence,
                                         uint8_t *response, size_t length);
bool force_feedback_script_slot_apply(ForceFeedbackScriptSlot *slot,
                                      ForceFeedbackScriptSlotCommand command);
void force_feedback_script_clock_tick(ForceFeedbackScriptClock *clock,
                                      ForceFeedbackRuntimeMode mode);

#endif
