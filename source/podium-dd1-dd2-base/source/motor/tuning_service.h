#ifndef OPENTEC_BASE_MOTOR_TUNING_SERVICE_H
#define OPENTEC_BASE_MOTOR_TUNING_SERVICE_H

#include <stdbool.h>

#include "motor/tuning_sync.h"

typedef struct {
    MotorTuningSync sync;
    MotorParameterWrite write;
    bool transfer_active;
} MotorTuningService;

void motor_tuning_service_init(MotorTuningService *service, const TuningProfile *profile,
                               const MotorTuningContext *context);
void motor_tuning_service_refresh(MotorTuningService *service, const TuningProfile *profile,
                                  const MotorTuningContext *context);
void motor_tuning_service_run(MotorTuningService *service);
bool motor_tuning_service_pending(const MotorTuningService *service);

#endif
