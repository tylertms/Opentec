#ifndef OPENTEC_BASE_MOTOR_TUNING_SERVICE_H
#define OPENTEC_BASE_MOTOR_TUNING_SERVICE_H

#include <stdbool.h>

#include "motor/tuning_sync.h"

/**
 * @brief Asynchronous motor tuning transfer state.
 *
 * Combines desired-parameter synchronization with the currently encoded auxiliary-bus write.
 */
typedef struct {
    MotorTuningSync sync;      /**< Desired and dirty motor tuning parameter state. */
    MotorParameterWrite write; /**< Parameter write selected for the active or next transfer. */
    bool transfer_active;      /**< True while the selected auxiliary-bus write is in progress. */
} MotorTuningService;

/**
 * @brief Initializes asynchronous motor tuning.
 *
 * Encodes the initial desired parameter set and clears auxiliary-bus transfer ownership.
 *
 * @param[out] service Motor tuning service state to initialize.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_service_init(MotorTuningService *service, const TuningProfile *profile,
                               const MotorTuningContext *context);

/**
 * @brief Refreshes desired motor tuning writes.
 *
 * Re-encodes the profile under the supplied runtime context and marks changed parameters for
 * transfer.
 *
 * @param[in,out] service Motor tuning service state.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_service_refresh(MotorTuningService *service, const TuningProfile *profile,
                                  const MotorTuningContext *context);

/**
 * @brief Advances asynchronous motor tuning transfers.
 *
 * Completes an active auxiliary-bus write and starts at most one pending parameter write when the
 * bus is idle.
 *
 * @param[in,out] service Motor tuning service and transfer state.
 */
void motor_tuning_service_run(MotorTuningService *service);

/**
 * @brief Reports whether motor tuning has outstanding work.
 *
 * Includes an active auxiliary-bus transfer and parameter writes waiting to start.
 *
 * @param[in] service Motor tuning service state.
 * @return True while motor tuning work remains outstanding.
 */
bool motor_tuning_service_pending(const MotorTuningService *service);

#endif
