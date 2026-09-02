#ifndef OPENTEC_BASE_COOLING_TEMPERATURE_H
#define OPENTEC_BASE_COOLING_TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Thermistor monitor dimensions.
 *
 * The monitor accumulates one primary and one secondary thermistor channel over a fixed sample
 * window before publishing converted temperatures and the native resistance-report values.
 */
enum {
    COOLING_TEMPERATURE_CHANNEL_COUNT = 2,   /**< Number of thermistor channels. */
    COOLING_TEMPERATURE_SAMPLE_COUNT = 1000, /**< Samples accumulated per conversion window. */
};

/**
 * @brief State for two-channel thermistor accumulation and conversion.
 *
 * Accumulators and sample_count represent the current partial window; temperatures_c contains the
 * most recently completed conversion for each channel.
 */
typedef struct {
    uint32_t
        adc_totals[COOLING_TEMPERATURE_CHANNEL_COUNT]; /**< ADC totals in the current window. */
    uint16_t sample_count; /**< Samples accumulated in the current window. */
    int16_t
        temperatures_c[COOLING_TEMPERATURE_CHANNEL_COUNT];         /**< Latest converted channel
                                                                      temperatures in degrees Celsius. */
    uint16_t resistance_values[COOLING_TEMPERATURE_CHANNEL_COUNT]; /**< Latest native diagnostic
                                                                       resistance values. */
} CoolingTemperatureMonitor;

/**
 * @brief Initializes a thermistor sampling window.
 *
 * Clears both channel accumulators, the sample count, and the published temperatures.
 *
 * @param[out] monitor Temperature sampling state to initialize.
 */
void cooling_temperature_monitor_init(CoolingTemperatureMonitor *monitor);

/**
 * @brief Accumulates one sample for each thermistor channel.
 *
 * Adds both ADC values to the current 1,000-sample window, converts and publishes both
 * temperatures when the window completes, and then clears the accumulators.
 *
 * @param[in,out] monitor Temperature accumulators and latest converted values.
 * @param[in] primary_adc Primary 12-bit thermistor ADC sample.
 * @param[in] secondary_adc Secondary 12-bit thermistor ADC sample.
 * @return True when a complete window was converted and cleared.
 */
bool cooling_temperature_monitor_add(CoolingTemperatureMonitor *monitor, uint16_t primary_adc,
                                     uint16_t secondary_adc);

/**
 * @brief Converts thermistor resistance through the board calibration curve.
 *
 * Linearly interpolates between adjacent five-degree resistance entries. Returns -99.9 degrees at
 * or above the highest table resistance and 999.9 degrees below the lowest table resistance.
 *
 * @param[in] resistance_ohms Thermistor resistance in ohms.
 * @return Interpolated temperature in degrees Celsius, or an out-of-range sentinel.
 */
float cooling_temperature_from_resistance(float resistance_ohms);

/**
 * @brief Converts a 1,000-sample thermistor ADC total to integer temperature.
 *
 * Averages the total, applies the board's 3.3-volt divider and 10-kilohm reference, then truncates
 * the calibrated temperature to int16_t.
 *
 * @param[in] adc_total Sum of 1,000 12-bit thermistor ADC samples.
 * @return Truncated calibrated temperature in degrees Celsius.
 */
int16_t cooling_temperature_from_adc_total(uint32_t adc_total);

/**
 * @brief Converts a 1,000-sample thermistor ADC total to a native resistance-report value.
 *
 * Applies the Fanatec voltage-divider equation and truncates the calibrated lookup result using
 * the unsigned representation used by native diagnostic bytes five through eight.
 *
 * @param[in] adc_total Sum of 1,000 12-bit thermistor ADC samples.
 * @return Native diagnostic resistance value.
 */
uint16_t cooling_resistance_value_from_adc_total(uint32_t adc_total);

#endif
