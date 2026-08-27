#include "motor/telemetry.h"

#include <stdbool.h>
#include <stdint.h>

static uint16_t read_u16(const uint8_t data[2]) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

void motor_telemetry_init(MotorTelemetry *telemetry) { *telemetry = (MotorTelemetry){0}; }

void motor_telemetry_set_motor_temperature(MotorTelemetry *telemetry, const uint8_t data[2]) {
    uint16_t value = read_u16(data);
    if (value != UINT16_MAX) {
        telemetry->motor_temperature = value;
        telemetry->motor_temperature_valid = true;
    }
}

void motor_telemetry_set_driver_temperature(MotorTelemetry *telemetry, const uint8_t data[2]) {
    uint16_t value = read_u16(data);
    if (value != UINT16_MAX) {
        telemetry->driver_temperature = value;
        telemetry->driver_temperature_valid = true;
    }
}

void motor_telemetry_set_runtime(MotorTelemetry *telemetry, const uint8_t data[4]) {
    uint32_t value = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
                     (uint32_t)data[3] << 24;
    if (value != UINT32_MAX) {
        telemetry->runtime_seconds = value;
        telemetry->runtime_valid = true;
    }
}

void motor_telemetry_set_accessory_type(MotorTelemetry *telemetry, uint8_t value) {
    if (value != UINT8_MAX) {
        telemetry->accessory_type = value;
        telemetry->accessory_type_valid = true;
    }
}
