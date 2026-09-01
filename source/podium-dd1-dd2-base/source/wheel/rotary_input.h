#ifndef OPENTEC_BASE_WHEEL_ROTARY_INPUT_H
#define OPENTEC_BASE_WHEEL_ROTARY_INPUT_H

#include <stdint.h>

/** @brief Number of rotary channels processed by WheelRotaryInput. */
enum {
    WHEEL_ROTARY_INPUT_CHANNEL_COUNT =
        4, /**< Number of rotary channels processed by WheelRotaryInput. */
};

/** @brief Event emitted for one rotary channel update. */
typedef enum {
    WHEEL_ROTARY_EVENT_NONE,     /**< No rotary step is currently emitted. */
    WHEEL_ROTARY_EVENT_FORWARD,  /**< One forward rotary step is currently emitted. */
    WHEEL_ROTARY_EVENT_BACKWARD, /**< One backward rotary step is currently emitted. */
} WheelRotaryEvent;

/** @brief Debounce phase for one rotary channel. */
typedef enum {
    WHEEL_ROTARY_PHASE_IDLE,    /**< The channel can emit a newly accumulated step. */
    WHEEL_ROTARY_PHASE_HOLD,    /**< The current event is held for its debounce interval. */
    WHEEL_ROTARY_PHASE_RELEASE, /**< The channel waits for its release interval to finish. */
} WheelRotaryPhase;

/** @brief Position, event, and debounce state for one rotary channel. */
typedef struct {
    uint32_t deadline_ms; /**< Monotonic deadline for the current hold or release phase. */
    int8_t pending_steps; /**< Signed steps accumulated after the current event. */
    uint8_t position; /**< Last tracked nonzero position, or UINT8_MAX before synchronization. */
    WheelRotaryEvent event; /**< Event currently emitted for this channel. */
    WheelRotaryPhase phase; /**< Current debounce phase. */
} WheelRotaryChannel;

/** @brief State for all wheel rotary channels. */
typedef struct {
    WheelRotaryChannel channels[WHEEL_ROTARY_INPUT_CHANNEL_COUNT]; /**< Per-channel rotary state. */
} WheelRotaryInput;

/**
 * @brief Initializes rotary input processing.
 *
 * Marks every channel as unsynchronized and clears its pending event state.
 *
 * @param[out] input Rotary input state to initialize.
 */
void wheel_rotary_input_init(WheelRotaryInput *input);

/**
 * @brief Updates one rotary input channel.
 *
 * Tracks the sampled position and advances the channel's hold and release debounce phases.
 * Position zero is treated as unavailable and does not change the tracked position or pending
 * step count; the debounce phase still advances.
 *
 * @param[in,out] input Rotary input state to update.
 * @param[in] channel Zero-based rotary channel index.
 * @param[in] position Latest rotary position, or zero when unavailable.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return The current channel event, or WHEEL_ROTARY_EVENT_NONE when input is null or channel is
 * out of range.
 */
WheelRotaryEvent wheel_rotary_input_update(WheelRotaryInput *input, uint8_t channel,
                                           uint8_t position, uint32_t now_ms);

#endif
