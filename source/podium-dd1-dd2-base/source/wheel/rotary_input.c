#include "wheel/rotary_input.h"

#include <stddef.h>
#include <stdint.h>

enum {
    ROTARY_POSITION_FIRST = 1,
    ROTARY_POSITION_LOW_WRAP_MAXIMUM = 3,
    ROTARY_POSITION_HIGH_WRAP_MINIMUM = 10,
    ROTARY_POSITION_LAST = 12,
    ROTARY_EVENT_HOLD_MS = 80,
    ROTARY_EVENT_RELEASE_MS = 80,
};

/**
 * @brief Adds one signed step with byte-width wrapping.
 *
 * Updates the pending-step accumulator without relying on signed overflow behavior.
 *
 * @param[in,out] pending_steps Signed pending-step accumulator.
 * @param[in] step Negative one or positive one.
 */
static void accumulate_step(int8_t *pending_steps, int8_t step) {
    *pending_steps = (int8_t)((uint8_t)*pending_steps + (uint8_t)step);
}

/**
 * @brief Advances a tracked rotary position toward its latest sample.
 *
 * Ignores position zero, captures the first nonzero sample without an event, and advances by one
 * position per update. Transitions between positions one through three and ten through twelve use
 * the twelve-position wrap direction.
 *
 * @param[in,out] channel Rotary channel position and pending-step state.
 * @param[in] position Latest nonzero position, or zero when unavailable.
 */
static void track_position(WheelRotaryChannel *channel, uint8_t position) {
    if (position == 0) {
        return;
    }
    if (channel->position == UINT8_MAX) {
        channel->position = position;
        return;
    }
    if (channel->position <= ROTARY_POSITION_LOW_WRAP_MAXIMUM &&
        position >= ROTARY_POSITION_HIGH_WRAP_MINIMUM) {
        channel->position = channel->position == ROTARY_POSITION_FIRST
                                ? ROTARY_POSITION_LAST
                                : (uint8_t)(channel->position - 1u);
        accumulate_step(&channel->pending_steps, -1);
    } else if (channel->position >= ROTARY_POSITION_HIGH_WRAP_MINIMUM &&
               position <= ROTARY_POSITION_LOW_WRAP_MAXIMUM) {
        channel->position = channel->position == ROTARY_POSITION_LAST
                                ? ROTARY_POSITION_FIRST
                                : (uint8_t)(channel->position + 1u);
        accumulate_step(&channel->pending_steps, 1);
    } else if (position < channel->position) {
        channel->position--;
        accumulate_step(&channel->pending_steps, -1);
    } else if (position > channel->position) {
        channel->position++;
        accumulate_step(&channel->pending_steps, 1);
    }
}

/**
 * @brief Clears pending rotary movement on every channel.
 *
 * Discards accumulated steps together when an emitted event finishes its hold interval.
 *
 * @param[in,out] input Four-channel rotary input state.
 */
static void clear_pending_steps(WheelRotaryInput *input) {
    for (uint8_t channel = 0; channel < WHEEL_ROTARY_INPUT_CHANNEL_COUNT; channel++) {
        input->channels[channel].pending_steps = 0;
    }
}

/**
 * @brief Initializes the multi-position rotary input processor.
 *
 * Marks all four channels as unsynchronized and clears their event, timing, and pending-step
 * state.
 *
 * @param[out] input Four-channel rotary input state to initialize.
 */
void wheel_rotary_input_init(WheelRotaryInput *input) {
    for (uint8_t channel = 0; channel < WHEEL_ROTARY_INPUT_CHANNEL_COUNT; channel++) {
        input->channels[channel] = (WheelRotaryChannel){.position = UINT8_MAX};
    }
}

/**
 * @brief Updates one multi-position rotary channel.
 *
 * Tracks one step toward the latest position, emits forward or backward event codes for eighty
 * milliseconds, and enforces an eighty-millisecond released interval before another event. The
 * end of a hold interval clears pending movement across all four channels.
 *
 * @param[in,out] input Four-channel rotary input state.
 * @param[in] channel Channel index from zero through three.
 * @param[in] position Latest rotary position, or zero when unavailable.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Current event code for the selected channel.
 */
WheelRotaryEvent wheel_rotary_input_update(WheelRotaryInput *input, uint8_t channel,
                                           uint8_t position, uint32_t now_ms) {
    if (input == NULL || channel >= WHEEL_ROTARY_INPUT_CHANNEL_COUNT) {
        return WHEEL_ROTARY_EVENT_NONE;
    }

    WheelRotaryChannel *state = &input->channels[channel];
    track_position(state, position);

    switch (state->phase) {
    case WHEEL_ROTARY_PHASE_IDLE:
        if (state->pending_steps == 0) {
            state->event = WHEEL_ROTARY_EVENT_NONE;
            break;
        }
        if (state->pending_steps > 0) {
            state->event = WHEEL_ROTARY_EVENT_FORWARD;
            accumulate_step(&state->pending_steps, -1);
        } else {
            state->event = WHEEL_ROTARY_EVENT_BACKWARD;
            accumulate_step(&state->pending_steps, 1);
        }
        state->deadline_ms = now_ms + ROTARY_EVENT_HOLD_MS;
        state->phase = WHEEL_ROTARY_PHASE_HOLD;
        break;
    case WHEEL_ROTARY_PHASE_HOLD:
        if (state->deadline_ms <= now_ms) {
            state->event = WHEEL_ROTARY_EVENT_NONE;
            state->deadline_ms = now_ms + ROTARY_EVENT_RELEASE_MS;
            state->phase = WHEEL_ROTARY_PHASE_RELEASE;
            clear_pending_steps(input);
        }
        break;
    case WHEEL_ROTARY_PHASE_RELEASE:
        if (state->deadline_ms <= now_ms) {
            state->event = WHEEL_ROTARY_EVENT_NONE;
            state->phase = WHEEL_ROTARY_PHASE_IDLE;
        }
        break;
    }
    return state->event;
}
