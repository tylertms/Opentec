#include "motor/telemetry.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Decodes a little-endian 16-bit telemetry value.
 *
 * Combines the low byte followed by the high byte used by the motor register interface.
 *
 * @param[in] data Two-byte motor register value.
 * @return Decoded unsigned value.
 */
static uint16_t read_u16(const uint8_t data[2]) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

/**
 * @brief Initializes motor telemetry storage.
 *
 * Clears all values and marks every telemetry channel unavailable.
 *
 * @param[out] telemetry Motor telemetry storage to initialize.
 */
void motor_telemetry_init(MotorTelemetry *telemetry) { *telemetry = (MotorTelemetry){0}; }

/**
 * @brief Publishes a valid little-endian motor-temperature response.
 *
 * Decodes the two-byte value and retains the previous telemetry when the response is 0xFFFF.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] data Two-byte temperature response.
 */
void motor_telemetry_set_motor_temperature(MotorTelemetry *telemetry, const uint8_t data[2]) {
    uint16_t value = read_u16(data);
    if (value != UINT16_MAX) {
        telemetry->motor_temperature = value;
        telemetry->motor_temperature_valid = true;
    }
}

/**
 * @brief Publishes a valid little-endian driver-temperature response.
 *
 * Decodes the two-byte value and retains the previous telemetry when the response is 0xFFFF.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] data Two-byte temperature response.
 */
void motor_telemetry_set_driver_temperature(MotorTelemetry *telemetry, const uint8_t data[2]) {
    uint16_t value = read_u16(data);
    if (value != UINT16_MAX) {
        telemetry->driver_temperature = value;
        telemetry->driver_temperature_valid = true;
    }
}

/**
 * @brief Publishes a valid little-endian motor-runtime response.
 *
 * Decodes the four-byte value and retains the previous telemetry when the response is 0xFFFFFFFF.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] data Four-byte runtime response.
 */
void motor_telemetry_set_runtime(MotorTelemetry *telemetry, const uint8_t data[4]) {
    uint32_t value = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
                     (uint32_t)data[3] << 24;
    if (value != UINT32_MAX) {
        telemetry->runtime_seconds = value;
        telemetry->runtime_valid = true;
    }
}

/**
 * @brief Publishes an available motor-accessory type response.
 *
 * Stores the accessory type and marks it available unless the response is 0xFF.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] value Accessory type response byte.
 */
void motor_telemetry_set_accessory_type(MotorTelemetry *telemetry, uint8_t value) {
    if (value != UINT8_MAX) {
        telemetry->accessory_type = value;
        telemetry->accessory_type_valid = true;
    }
}
