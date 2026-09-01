#ifndef OPENTEC_BASE_MOTOR_TELEMETRY_H
#define OPENTEC_BASE_MOTOR_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Latest motor-controller telemetry values and availability.
 *
 * Retains the last accepted value for each channel and records whether that channel has received a
 * response other than its protocol-specific unavailable sentinel.
 */
typedef struct {
    uint16_t motor_temperature;    /**< Latest motor-temperature register value. */
    uint16_t driver_temperature;   /**< Latest driver-temperature register value. */
    uint32_t runtime_seconds;      /**< Latest motor runtime in seconds. */
    uint8_t accessory_type;        /**< Latest motor accessory-type value. */
    bool motor_temperature_valid;  /**< True after a valid motor-temperature response. */
    bool driver_temperature_valid; /**< True after a valid driver-temperature response. */
    bool runtime_valid;            /**< True after a valid runtime response. */
    bool accessory_type_valid;     /**< True after a valid accessory-type response. */
} MotorTelemetry;

/**
 * @brief Initializes motor telemetry storage.
 *
 * Clears all values and marks every telemetry channel unavailable.
 *
 * @param[out] telemetry Motor telemetry storage to initialize.
 */
void motor_telemetry_init(MotorTelemetry *telemetry);

/**
 * @brief Stores a motor-temperature response.
 *
 * Decodes the little-endian response and marks the channel valid unless the response is its
 * unavailable sentinel.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] data Two-byte motor-temperature response.
 */
void motor_telemetry_set_motor_temperature(MotorTelemetry *telemetry, const uint8_t data[2]);

/**
 * @brief Stores a driver-temperature response.
 *
 * Decodes the little-endian response and marks the channel valid unless the response is its
 * unavailable sentinel.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] data Two-byte driver-temperature response.
 */
void motor_telemetry_set_driver_temperature(MotorTelemetry *telemetry, const uint8_t data[2]);

/**
 * @brief Stores a motor-runtime response.
 *
 * Decodes the four-byte little-endian response and marks the channel valid unless the response is
 * its unavailable sentinel.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] data Four-byte motor-runtime response.
 */
void motor_telemetry_set_runtime(MotorTelemetry *telemetry, const uint8_t data[4]);

/**
 * @brief Stores a motor accessory-type response.
 *
 * Retains the response and marks the channel valid unless the value is its unavailable sentinel.
 *
 * @param[in,out] telemetry Motor telemetry values and validity flags.
 * @param[in] value Accessory-type response byte.
 */
void motor_telemetry_set_accessory_type(MotorTelemetry *telemetry, uint8_t value);

#endif
