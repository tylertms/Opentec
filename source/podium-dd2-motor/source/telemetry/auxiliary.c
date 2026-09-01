#include "telemetry/auxiliary.h"

#include <limits.h>

/**
 * @brief Fixed sizes used by auxiliary ADC accumulation and conversion tables.
 */
enum {
    MOTOR_AUXILIARY_SAMPLE_COUNT = 10000, /**< Samples in one published auxiliary window. */
    MOTOR_TEMPERATURE_TABLE_LENGTH = 32, /**< Entries in each temperature conversion table. */
};

/**
 * @brief Motor-temperature ADC conversion table.
 */
static const uint16_t motor_temperature_table[MOTOR_TEMPERATURE_TABLE_LENGTH] = {
    3987U, 3953U, 3911U, 3860U, 3796U, 3722U, 3634U, 3531U, 3413U, 3281U, 3136U,
    2978U, 2809U, 2633U, 2453U, 2272U, 2089U, 1913U, 1742U, 1580U, 1428U, 1287U,
    1157U, 1037U, 929U,  832U,  744U,  666U,  597U,  534U,  478U,  429U,
};

/**
 * @brief Motor-driver-temperature ADC conversion table.
 */
static const uint16_t driver_temperature_table[MOTOR_TEMPERATURE_TABLE_LENGTH] = {
    3948U, 3911U, 3867U, 3815U, 3754U, 3684U, 3604U, 3513U, 3413U, 3301U, 3180U,
    3050U, 2911U, 2766U, 2615U, 2462U, 2309U, 2158U, 2008U, 1862U, 1723U, 1589U,
    1461U, 1341U, 1229U, 1125U, 1029U, 941U,  859U,  848U,  781U,  719U,
};

int16_t motor_temperature_interpolate(uint16_t sample, MotorTemperatureSensor sensor) {
    const uint16_t *table =
        sensor == kMotorTemperatureMotor ? motor_temperature_table : driver_temperature_table;
    uint32_t index = 0U;
    while (index < MOTOR_TEMPERATURE_TABLE_LENGTH && sample < table[index]) {
        ++index;
    }

    if (index == 0U) {
        return -255;
    }
    if (index == MOTOR_TEMPERATURE_TABLE_LENGTH) {
        return 255;
    }

    uint32_t upper = table[index - 1U];
    uint32_t lower = table[index];
    uint32_t numerator = upper - sample;
    uint32_t denominator = upper - lower;
    uint32_t fraction = numerator < denominator ? (numerator << 15U) / denominator : INT16_MAX;
    uint32_t position = ((index - 1U) << 15U) + fraction;
    return (int16_t)((int32_t)(position * 5U >> 15U) - 15);
}

bool motor_auxiliary_samples_accumulate(MotorAuxiliaryAccumulator *accumulator, uint16_t motor,
                                        uint16_t driver) {
    accumulator->motor_sum += motor;
    accumulator->driver_sum += driver;
    ++accumulator->count;
    if (accumulator->count < MOTOR_AUXILIARY_SAMPLE_COUNT) {
        return false;
    }

    accumulator->published_motor_sum = accumulator->motor_sum;
    accumulator->published_driver_sum = accumulator->driver_sum;
    accumulator->motor_sum = 0U;
    accumulator->driver_sum = 0U;
    accumulator->count = 0U;
    accumulator->ready = true;
    return true;
}

bool motor_auxiliary_samples_resolve(MotorAuxiliaryAccumulator *accumulator,
                                     MotorAuxiliaryTelemetry *telemetry) {
    if (!accumulator->ready) {
        return false;
    }

    accumulator->ready = false;
    telemetry->motor_average =
        (uint16_t)(accumulator->published_motor_sum / MOTOR_AUXILIARY_SAMPLE_COUNT);
    telemetry->driver_average =
        (uint16_t)(accumulator->published_driver_sum / MOTOR_AUXILIARY_SAMPLE_COUNT);
    telemetry->motor_temperature =
        motor_temperature_interpolate(telemetry->motor_average, kMotorTemperatureMotor);
    telemetry->driver_temperature =
        motor_temperature_interpolate(telemetry->driver_average, kMotorTemperatureDriver);
    return true;
}
