#ifndef OPENTEC_MOTOR_TELEMETRY_H
#define OPENTEC_MOTOR_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Selects the temperature conversion table for an auxiliary ADC channel.
 */
typedef enum {
    kMotorTemperatureMotor, /**< Motor-temperature sensor table. */
    kMotorTemperatureDriver, /**< Motor-driver-temperature sensor table. */
} MotorTemperatureSensor;

/**
 * @brief Stores active and completed auxiliary ADC accumulation windows.
 */
typedef struct {
    uint32_t motor_sum; /**< Running sum of motor-temperature ADC samples. */
    uint32_t driver_sum; /**< Running sum of motor-driver-temperature ADC samples. */
    uint32_t published_motor_sum; /**< Completed motor-temperature sum awaiting resolution. */
    uint32_t published_driver_sum; /**< Completed driver-temperature sum awaiting resolution. */
    uint32_t count; /**< Number of samples in the active accumulation window. */
    bool ready; /**< True when a completed window is ready for resolution. */
} MotorAuxiliaryAccumulator;

/**
 * @brief Stores resolved auxiliary ADC averages and temperatures.
 */
typedef struct {
    uint16_t motor_average; /**< Averaged motor-temperature ADC sample. */
    uint16_t driver_average; /**< Averaged motor-driver-temperature ADC sample. */
    int16_t motor_temperature; /**< Converted motor temperature in degrees Celsius. */
    int16_t driver_temperature; /**< Converted motor-driver temperature in degrees Celsius. */
} MotorAuxiliaryTelemetry;

/**
 * @brief Converts an auxiliary ADC average to a temperature.
 *
 * Selects the motor or driver table, interpolates between adjacent entries, and returns an
 * endpoint sentinel when the sample is outside the supported table range.
 *
 * @param[in] sample Averaged twelve-bit ADC sample.
 * @param[in] sensor Temperature-table selection.
 * @return Interpolated temperature, or -255 or 255 for a sample outside the table range.
 */
int16_t motor_temperature_interpolate(uint16_t sample, MotorTemperatureSensor sensor);

/**
 * @brief Accumulates one pair of auxiliary ADC samples.
 *
 * The completed ten-thousand-sample window is published and the active accumulation restarts when
 * the sample count reaches its configured limit.
 *
 * @param[in,out] accumulator Accumulation and publication state to update.
 * @param[in] motor Motor-temperature ADC sample.
 * @param[in] driver Motor-driver-temperature ADC sample.
 * @return True when a completed window was published; otherwise false.
 */
bool motor_auxiliary_samples_accumulate(MotorAuxiliaryAccumulator *accumulator, uint16_t motor,
                                        uint16_t driver);

/**
 * @brief Resolves one completed auxiliary ADC window.
 *
 * Computes both channel averages and converts them through their corresponding temperature tables.
 *
 * @param[in,out] accumulator Accumulation and publication state to update.
 * @param[out] telemetry Resolved averages and temperatures.
 * @return True when a published window was consumed; otherwise false.
 */
bool motor_auxiliary_samples_resolve(MotorAuxiliaryAccumulator *accumulator,
                                     MotorAuxiliaryTelemetry *telemetry);

#endif
