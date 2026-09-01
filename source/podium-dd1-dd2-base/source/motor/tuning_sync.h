#ifndef OPENTEC_BASE_MOTOR_TUNING_SYNC_H
#define OPENTEC_BASE_MOTOR_TUNING_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/tuning_parameter.h"

/**
 * @brief Motor tuning parameter synchronization state.
 *
 * Tracks encoded desired writes, dirty and valid parameter masks, the rotating selection cursor,
 * and one in-flight auxiliary-bus write.
 */
typedef struct {
    MotorParameterWrite
        desired[MOTOR_TUNING_PARAMETER_COUNT]; /**< Latest desired encoded writes. */
    MotorParameterWrite in_flight_write; /**< Copy of the write currently awaiting completion. */
    uint16_t valid_parameters;           /**< Mask of parameters with valid desired writes. */
    uint16_t dirty_parameters;           /**< Mask of parameters that still require transmission. */
    uint8_t next_parameter;              /**< Cursor for selecting the next dirty parameter. */
    uint8_t in_flight_parameter;         /**< Parameter index associated with in_flight_write. */
    bool in_flight;                      /**< True while one parameter write awaits completion. */
} MotorTuningSync;

/**
 * @brief Initializes motor tuning synchronization.
 *
 * Clears transfer progress and encodes the initial desired parameter set for the supplied profile
 * and runtime context.
 *
 * @param[out] sync Motor tuning synchronization state to initialize.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_sync_init(MotorTuningSync *sync, const TuningProfile *profile,
                            const MotorTuningContext *context);

/**
 * @brief Refreshes desired motor tuning parameters.
 *
 * Re-encodes each supported parameter and marks changed or newly valid writes dirty for transfer.
 *
 * @param[in,out] sync Motor tuning synchronization state.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_sync_refresh(MotorTuningSync *sync, const TuningProfile *profile,
                               const MotorTuningContext *context);

/**
 * @brief Selects the next dirty motor tuning write.
 *
 * Records one dirty parameter as in flight and copies its encoded write to the output while no
 * other write is active.
 *
 * @param[in,out] sync Motor tuning synchronization state.
 * @param[out] write Destination for the selected motor parameter write.
 * @return True when a dirty parameter was selected; otherwise false.
 */
bool motor_tuning_sync_next(MotorTuningSync *sync, MotorParameterWrite *write);

/**
 * @brief Completes the current motor tuning write.
 *
 * Clears the selected parameter's dirty bit only when the transfer succeeded and the desired value
 * remained unchanged; failed or superseded writes remain pending.
 *
 * @param[in,out] sync Motor tuning synchronization state.
 * @param[in] succeeded True when the motor parameter transfer completed successfully.
 */
void motor_tuning_sync_complete(MotorTuningSync *sync, bool succeeded);

/**
 * @brief Reports whether motor tuning synchronization has outstanding work.
 *
 * Includes an in-flight transfer and dirty parameters waiting for transmission.
 *
 * @param[in] sync Motor tuning synchronization state.
 * @return True while a transfer or dirty parameter remains outstanding.
 */
bool motor_tuning_sync_pending(const MotorTuningSync *sync);

#endif
