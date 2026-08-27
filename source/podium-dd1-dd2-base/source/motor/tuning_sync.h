#ifndef OPENTEC_BASE_MOTOR_TUNING_SYNC_H
#define OPENTEC_BASE_MOTOR_TUNING_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/tuning_parameter.h"

typedef struct {
    MotorParameterWrite desired[MOTOR_TUNING_PARAMETER_COUNT];
    MotorParameterWrite in_flight_write;
    uint16_t valid_parameters;
    uint16_t dirty_parameters;
    uint8_t next_parameter;
    uint8_t in_flight_parameter;
    bool in_flight;
} MotorTuningSync;

void motor_tuning_sync_init(MotorTuningSync *sync, const TuningProfile *profile,
                            const MotorTuningContext *context);
void motor_tuning_sync_refresh(MotorTuningSync *sync, const TuningProfile *profile,
                               const MotorTuningContext *context);
bool motor_tuning_sync_next(MotorTuningSync *sync, MotorParameterWrite *write);
void motor_tuning_sync_complete(MotorTuningSync *sync, bool succeeded);
bool motor_tuning_sync_pending(const MotorTuningSync *sync);

#endif
