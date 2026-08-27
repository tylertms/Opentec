#include "motor/tuning_service.h"

#include <stdbool.h>

#include "platform/aux_bus.h"

enum { MOTOR_AUX_BUS_ADDRESS = 0x78 };

void motor_tuning_service_init(MotorTuningService *service, const TuningProfile *profile,
                               const MotorTuningContext *context) {
    motor_tuning_sync_init(&service->sync, profile, context);
    service->transfer_active = false;
}

void motor_tuning_service_refresh(MotorTuningService *service, const TuningProfile *profile,
                                  const MotorTuningContext *context) {
    motor_tuning_sync_refresh(&service->sync, profile, context);
}

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

bool motor_tuning_service_pending(const MotorTuningService *service) {
    return service->transfer_active || motor_tuning_sync_pending(&service->sync);
}
