#include "motor/tuning_service.h"

#include <stdbool.h>

#include "platform/aux_bus.h"

/**
 * @brief Auxiliary-bus address used for motor tuning writes.
 */
enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78 /**< Auxiliary-bus address of the motor controller. */
};

/**
 * @brief Initializes the asynchronous motor tuning service.
 *
 * Builds the initial desired parameter set and clears bus transfer ownership.
 *
 * @param[out] service Motor tuning service to initialize.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_service_init(MotorTuningService *service, const TuningProfile *profile,
                               const MotorTuningContext *context) {
    motor_tuning_sync_init(&service->sync, profile, context);
    service->transfer_active = false;
}

/**
 * @brief Refreshes motor tuning writes after settings or runtime context change.
 *
 * Re-encodes the desired parameter set and leaves unchanged values synchronized.
 *
 * @param[in,out] service Motor tuning service to refresh.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 */
void motor_tuning_service_refresh(MotorTuningService *service, const TuningProfile *profile,
                                  const MotorTuningContext *context) {
    motor_tuning_sync_refresh(&service->sync, profile, context);
}

/**
 * @brief Advances the asynchronous motor tuning transfer.
 *
 * Completes the active auxiliary-bus write, records its result, and starts at most one pending
 * write to motor address 0x78 when the bus is idle.
 *
 * @param[in,out] service Motor tuning service and transfer state to advance.
 */
void motor_tuning_service_run(MotorTuningService *service) {
    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (service->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }

        motor_tuning_sync_complete(&service->sync, bus_status == PLATFORM_AUX_BUS_SUCCEEDED);
        platform_aux_bus_clear();
        service->transfer_active = false;
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }

    if (bus_status != PLATFORM_AUX_BUS_IDLE ||
        !motor_tuning_sync_next(&service->sync, &service->write)) {
        return;
    }

    service->transfer_active = platform_aux_bus_start_write(
        MOTOR_AUX_BUS_ADDRESS, service->write.address, service->write.data, service->write.length);
    if (!service->transfer_active) {
        motor_tuning_sync_complete(&service->sync, false);
    }
}

/**
 * @brief Reports whether the motor tuning service has outstanding work.
 *
 * Includes both an active auxiliary-bus transfer and parameter writes waiting to start.
 *
 * @param[in] service Motor tuning service state.
 * @return True while any motor tuning write remains outstanding.
 */
bool motor_tuning_service_pending(const MotorTuningService *service) {
    return service->transfer_active || motor_tuning_sync_pending(&service->sync);
}
