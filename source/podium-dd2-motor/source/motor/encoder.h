#ifndef OPENTEC_MOTOR_ENCODER_H
#define OPENTEC_MOTOR_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Results from publishing an extended encoder position. */
typedef enum {
    kMotorEncoderPositionPending, /**< An encoder overflow is awaiting handling. */
    kMotorEncoderPositionUpdated, /**< The extended position was published in range. */
    kMotorEncoderPositionOutOfRange, /**< The published position reached a safety boundary. */
} MotorEncoderPositionResult;

/** @brief Extended encoder position and captured index-zero state. */
typedef struct {
    int32_t revolution_offset; /**< Signed offset accumulated across encoder overflows. */
    int32_t position; /**< Latest extended encoder position. */
    int32_t zero_counter; /**< Captured hardware counter at encoder zero. */
} MotorEncoderState;

/** @brief Phases of the two-index encoder-direction diagnostic. */
typedef enum {
    kMotorEncoderDirectionBegin, /**< Capture the start position and begin index search. */
    kMotorEncoderDirectionFirstIndex, /**< Seek and capture the first index. */
    kMotorEncoderDirectionSecondIndex, /**< Seek and validate the second index. */
    kMotorEncoderDirectionReturn, /**< Drive back toward the recorded start position. */
} MotorEncoderDirectionPhase;

/** @brief Results reported by the encoder-direction diagnostic. */
typedef enum {
    kMotorEncoderDirectionPending, /**< The direction sequence is still active. */
    kMotorEncoderDirectionPassed, /**< The shaft returned to the recorded start position. */
    kMotorEncoderDirectionFailed, /**< The measured index spacing was outside tolerance. */
} MotorEncoderDirectionResult;

/** @brief Persistent state for the encoder-direction diagnostic. */
typedef struct {
    MotorEncoderDirectionPhase phase; /**< Current diagnostic phase. */
    int32_t start_position; /**< Position captured when the diagnostic began. */
    int32_t first_index_position; /**< Position captured at the first index. */
    uint16_t status; /**< Product status word published during the diagnostic. */
} MotorEncoderDirectionState;

/** @brief Actions and result produced by one direction-diagnostic step. */
typedef struct {
    MotorEncoderDirectionResult result; /**< Direction-diagnostic result. */
    int16_t drive_current; /**< Current command for the active index search or return. */
    uint16_t status; /**< Status word to publish. */
    bool reset_controller; /**< True when the velocity controller must be reset. */
    bool restart_index_seek; /**< True when the index search timer must restart. */
} MotorEncoderDirectionStep;

/** @brief Result of one encoder-index search step. */
typedef struct {
    int16_t drive_current; /**< Fixed search current while the search remains active. */
    bool countdown_active; /**< True while the timeout countdown remains nonzero. */
    bool complete; /**< True when an index was detected or the search timed out. */
} MotorEncoderIndexSeekStep;

/**
 * @brief Applies one encoder-overflow revolution adjustment.
 *
 * The encoder modulus is added for increasing motion and subtracted for decreasing motion.
 *
 * @param[in,out] state Extended encoder state to update.
 * @param[in] modulus Encoder counts per revolution.
 * @param[in] increasing True when the overflow direction is increasing.
 */
void motor_encoder_overflow_apply(MotorEncoderState *state, int32_t modulus, bool increasing);

/**
 * @brief Updates the extended encoder position when overflow handling is complete.
 *
 * A pending overflow defers the position calculation; otherwise the counter, zero, and revolution
 * offset are combined and checked against the exclusive position limit.
 *
 * @param[in,out] state Extended encoder state to update.
 * @param[in] overflow_pending True while an overflow interrupt remains unhandled.
 * @param[in] counter Current sixteen-bit encoder counter.
 * @param[in] position_limit Exclusive positive and negative safety limit.
 * @return Pending, updated, or out-of-range position result.
 */
MotorEncoderPositionResult motor_encoder_position_update(MotorEncoderState *state,
                                                         bool overflow_pending, uint16_t counter,
                                                         int32_t position_limit);

/**
 * @brief Resets the extended encoder offset and published position.
 *
 * The captured zero counter is preserved.
 *
 * @param[in,out] state Extended encoder state to reset while preserving its zero counter.
 */
void motor_encoder_position_reset(MotorEncoderState *state);

/**
 * @brief Computes encoder position relative to the captured zero counter.
 *
 * Negative counter differences wrap once by the supplied encoder modulus.
 *
 * @param[in] counter Current hardware encoder counter.
 * @param[in] zero_counter Captured zero counter.
 * @param[in] modulus Encoder counts per revolution.
 * @return Unsigned position relative to zero.
 */
uint16_t motor_encoder_relative_position(uint16_t counter, uint16_t zero_counter, uint16_t modulus);

/**
 * @brief Advances one encoder-index search step.
 *
 * The fixed search current is returned until an index is detected or the timeout reaches zero.
 *
 * @param[in] index_detected True when the index interrupt captured an index.
 * @param[in] timeout_remaining Remaining search countdown.
 * @return Search current, countdown state, and completion flag.
 */
MotorEncoderIndexSeekStep motor_encoder_index_seek_step(bool index_detected,
                                                        uint16_t timeout_remaining);

/**
 * @brief Initializes the encoder-direction diagnostic state.
 *
 * The next step captures the starting position and arms the first index search.
 *
 * @param[out] state Direction-diagnostic state to clear.
 */
void motor_encoder_direction_initialize(MotorEncoderDirectionState *state);

/**
 * @brief Advances the two-index encoder-direction diagnostic.
 *
 * The sequence compares two index positions one revolution apart and then reaches or passes its
 * starting position before reporting pass or failure.
 *
 * @param[in,out] state Persistent direction-diagnostic state to update.
 * @param[in] index_seek_complete True when the active index search has completed.
 * @param[in] position Current extended encoder position.
 * @param[in] encoder_modulus Expected encoder counts per revolution.
 * @return Diagnostic result and actions for current, status, controller, and index search.
 */
MotorEncoderDirectionStep motor_encoder_direction_check_step(MotorEncoderDirectionState *state,
                                                             bool index_seek_complete,
                                                             int32_t position,
                                                             int32_t encoder_modulus);

#endif
