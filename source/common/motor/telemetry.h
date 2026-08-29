#ifndef OPENTEC_MOTOR_TELEMETRY_H
#define OPENTEC_MOTOR_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    kMotorTemperaturePrimary,
    kMotorTemperatureSecondary,
} MotorTemperatureSensor;

typedef struct {
    uint32_t primary_sum;
    uint32_t secondary_sum;
    uint32_t published_primary_sum;
    uint32_t published_secondary_sum;
    uint32_t count;
    bool ready;
} MotorAuxiliaryAccumulator;

typedef struct {
    uint16_t primary_average;
    uint16_t secondary_average;
    int16_t primary_temperature;
    int16_t secondary_temperature;
} MotorAuxiliaryTelemetry;

int16_t motor_temperature_interpolate(uint16_t sample, MotorTemperatureSensor sensor);
bool motor_auxiliary_samples_accumulate(MotorAuxiliaryAccumulator *accumulator, uint16_t primary,
                                        uint16_t secondary);
bool motor_auxiliary_samples_resolve(MotorAuxiliaryAccumulator *accumulator,
                                     MotorAuxiliaryTelemetry *telemetry);

#endif
