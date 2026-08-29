#ifndef OPENTEC_MOTOR_TELEMETRY_H
#define OPENTEC_MOTOR_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    kMotorTemperatureMotor,
    kMotorTemperatureDriver,
} MotorTemperatureSensor;

typedef struct {
    uint32_t motor_sum;
    uint32_t driver_sum;
    uint32_t published_motor_sum;
    uint32_t published_driver_sum;
    uint32_t count;
    bool ready;
} MotorAuxiliaryAccumulator;

typedef struct {
    uint16_t motor_average;
    uint16_t driver_average;
    int16_t motor_temperature;
    int16_t driver_temperature;
} MotorAuxiliaryTelemetry;

int16_t motor_temperature_interpolate(uint16_t sample, MotorTemperatureSensor sensor);
bool motor_auxiliary_sample_due(uint8_t *conversion_count);
bool motor_auxiliary_samples_accumulate(MotorAuxiliaryAccumulator *accumulator, uint16_t motor,
                                        uint16_t driver);
bool motor_auxiliary_samples_resolve(MotorAuxiliaryAccumulator *accumulator,
                                     MotorAuxiliaryTelemetry *telemetry);

#endif
