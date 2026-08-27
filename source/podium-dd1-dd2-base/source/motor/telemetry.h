#ifndef OPENTEC_BASE_MOTOR_TELEMETRY_H
#define OPENTEC_BASE_MOTOR_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t motor_temperature;
    uint16_t driver_temperature;
    uint32_t runtime_seconds;
    uint8_t accessory_type;
    bool motor_temperature_valid;
    bool driver_temperature_valid;
    bool runtime_valid;
    bool accessory_type_valid;
} MotorTelemetry;

void motor_telemetry_init(MotorTelemetry *telemetry);
void motor_telemetry_set_motor_temperature(MotorTelemetry *telemetry, const uint8_t data[2]);
void motor_telemetry_set_driver_temperature(MotorTelemetry *telemetry, const uint8_t data[2]);
void motor_telemetry_set_runtime(MotorTelemetry *telemetry, const uint8_t data[4]);
void motor_telemetry_set_accessory_type(MotorTelemetry *telemetry, uint8_t value);

#endif
