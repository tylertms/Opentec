#ifndef OPENTEC_BASE_WHEEL_ROTARY_INPUT_H
#define OPENTEC_BASE_WHEEL_ROTARY_INPUT_H

#include <stdint.h>

enum {
    WHEEL_ROTARY_INPUT_CHANNEL_COUNT = 4,
};

typedef enum {
    WHEEL_ROTARY_EVENT_NONE,
    WHEEL_ROTARY_EVENT_FORWARD,
    WHEEL_ROTARY_EVENT_BACKWARD,
} WheelRotaryEvent;

typedef enum {
    WHEEL_ROTARY_PHASE_IDLE,
    WHEEL_ROTARY_PHASE_HOLD,
    WHEEL_ROTARY_PHASE_RELEASE,
} WheelRotaryPhase;

typedef struct {
    uint32_t deadline_ms;
    int8_t pending_steps;
    uint8_t position;
    WheelRotaryEvent event;
    WheelRotaryPhase phase;
} WheelRotaryChannel;

typedef struct {
    WheelRotaryChannel channels[WHEEL_ROTARY_INPUT_CHANNEL_COUNT];
} WheelRotaryInput;

void wheel_rotary_input_init(WheelRotaryInput *input);
WheelRotaryEvent wheel_rotary_input_update(WheelRotaryInput *input, uint8_t channel,
                                           uint8_t position, uint32_t now_ms);

#endif
