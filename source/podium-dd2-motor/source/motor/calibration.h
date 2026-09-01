#ifndef OPENTEC_MOTOR_CALIBRATION_H
#define OPENTEC_MOTOR_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of ready ADC samples averaged for each current phase. */
#define MOTOR_CURRENT_CALIBRATION_SAMPLE_COUNT 1024U

/** @brief Published average current offsets for the two measured phases. */
typedef struct {
    int16_t phase_a; /**< Average offset captured from phase A. */
    int16_t phase_b; /**< Average offset captured from phase B. */
} MotorCurrentOffsets;

/** @brief Stages of the two-phase current-offset calibration sequence. */
typedef enum {
    kMotorCurrentCalibrationIdle = 0, /**< Calibration is idle and has no active accumulation. */
    kMotorCurrentCalibrationPhaseA = 1, /**< Phase A samples are being accumulated. */
    kMotorCurrentCalibrationPhaseB = 2, /**< Phase B samples are being accumulated. */
    kMotorCurrentCalibrationComplete = 3, /**< Both phase averages have been published. */
} MotorCurrentCalibrationStage;

/** @brief Results reported while advancing current-offset calibration. */
typedef enum {
    kMotorCurrentCalibrationPending, /**< The active phase still needs ready samples. */
    kMotorCurrentCalibrationPhaseBStarted, /**< Phase A finished and phase B started. */
    kMotorCurrentCalibrationFinished, /**< Phase B finished and calibration completed. */
} MotorCurrentCalibrationResult;

/** @brief Persistent accumulators and published offsets for current calibration. */
typedef struct {
    MotorCurrentOffsets offsets; /**< Last published phase offsets. */
    MotorCurrentCalibrationStage stage; /**< Current calibration stage. */
    uint16_t sample_count; /**< Number of ready samples in the active phase. */
    uint32_t sample_sum; /**< Sum of ready ADC samples in the active phase. */
} MotorCurrentCalibrationState;

/**
 * @brief Starts current-offset calibration without discarding published offsets.
 *
 * The next ready samples are accumulated for phase A, and the sample count and sum are reset.
 *
 * @param[in,out] state Calibration state to initialize for phase A while preserving its offsets.
 */
void motor_current_calibration_start(MotorCurrentCalibrationState *state);

/**
 * @brief Accumulates one current-offset calibration sample.
 *
 * Each phase publishes the integer average of 1024 ready samples before advancing or completing.
 *
 * @param[in,out] state Calibration stage, accumulators, and published offsets.
 * @param[in] sample_ready True when sample contains a completed ADC conversion.
 * @param[in] sample Raw unsigned ADC conversion result.
 * @return Pending, phase-B started, or calibration finished.
 */
MotorCurrentCalibrationResult motor_current_calibration_step(MotorCurrentCalibrationState *state,
                                                             bool sample_ready, uint16_t sample);

#endif
