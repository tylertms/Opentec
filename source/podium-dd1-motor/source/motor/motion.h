#ifndef OPENTEC_MOTOR_MOTION_H
#define OPENTEC_MOTOR_MOTION_H

#include <stdint.h>

/** @brief Persistent state for a leaky fixed-point motion filter. */
typedef struct {
    int32_t accumulator; /**< Signed accumulated filter state. */
    uint16_t shift; /**< Right-shift count used to publish the filtered sample. */
} MotorMotionFilter;

/** @brief Persistent history for encoder and filtered-velocity differences. */
typedef struct {
    uint32_t previous_counter; /**< Previous hardware encoder counter. */
    int16_t previous_filtered_delta; /**< Previous filtered position delta. */
} MotorMotionState;

/** @brief Raw and filtered position and velocity samples from one estimator update. */
typedef struct {
    int16_t position_delta; /**< Scaled raw encoder-counter delta. */
    int16_t filtered_position_delta; /**< Leaky-filtered position delta. */
    int16_t velocity_delta; /**< Scaled difference between filtered position deltas. */
    int16_t filtered_velocity_delta; /**< Leaky-filtered velocity delta. */
} MotorMotionSample;

/**
 * @brief Applies a fixed-point scale with signed saturation.
 *
 * The mixed-width product is limited to the signed sixteen-bit range.
 *
 * @param[in] scale Unsigned fixed-point scale.
 * @param[in] value Signed value to scale.
 * @return Scaled value limited to the signed sixteen-bit range.
 */
int16_t motor_q15_scale_saturate(uint32_t scale, int16_t value);

/**
 * @brief Applies a fixed-point scale with signed sixteen-bit wrapping.
 *
 * The mixed-width product is truncated to its low sixteen bits.
 *
 * @param[in] scale Unsigned fixed-point scale.
 * @param[in] value Signed value to scale.
 * @return Low sixteen bits of the scaled value interpreted as signed.
 */
int16_t motor_q15_scale_wrap(uint32_t scale, int16_t value);

/**
 * @brief Subtracts two signed samples with sixteen-bit saturation.
 *
 * The current value is reduced by the previous value and the result is limited to signed endpoints.
 *
 * @param[in] value Current signed sample.
 * @param[in] previous Previous signed sample.
 * @return Saturated signed difference.
 */
int16_t motor_signed_difference_saturate(int16_t value, int16_t previous);

/**
 * @brief Advances a leaky fixed-point motion filter.
 *
 * The sample is accumulated, shifted for output, and the output is removed from the accumulator.
 *
 * @param[in,out] filter Filter state to update.
 * @param[in] sample Signed input sample.
 * @return Saturated filtered sample.
 */
int16_t motor_motion_filter_step(MotorMotionFilter *filter, int16_t sample);

/**
 * @brief Scales the wrapped difference between encoder counter samples.
 *
 * The previous counter is replaced with counter after the unsigned sixteen-bit subtraction.
 *
 * @param[in,out] state Encoder-difference history to update.
 * @param[in] counter Current hardware counter.
 * @param[in] scale Fixed-point scale for the counter delta.
 * @return Saturated scaled counter delta.
 */
int16_t motor_encoder_delta_scale(MotorMotionState *state, uint32_t counter, uint32_t scale);

/**
 * @brief Scales the difference between successive filtered position samples.
 *
 * The previous filtered delta is replaced with filtered_delta before scaling the difference.
 *
 * @param[in,out] state Filtered-position history to update.
 * @param[in] filtered_delta Current filtered position delta.
 * @param[in] scale Fixed-point scale for the velocity delta.
 * @return Saturated scaled velocity delta.
 */
int16_t motor_velocity_delta_scale(MotorMotionState *state, int16_t filtered_delta, uint32_t scale);

/**
 * @brief Produces raw and filtered position and velocity deltas.
 *
 * The counter delta feeds the position filter, and its filtered output feeds the velocity filter.
 *
 * @param[in,out] state Estimator history to update.
 * @param[in,out] position_filter Position-delta filter to update.
 * @param[in,out] velocity_filter Velocity-delta filter to update.
 * @param[in] counter Current hardware encoder counter.
 * @param[in] scale Fixed-point scale used for both deltas.
 * @return Raw and filtered position and velocity samples.
 */
MotorMotionSample motor_motion_sample(MotorMotionState *state, MotorMotionFilter *position_filter,
                                      MotorMotionFilter *velocity_filter, uint32_t counter,
                                      uint32_t scale);

#endif
