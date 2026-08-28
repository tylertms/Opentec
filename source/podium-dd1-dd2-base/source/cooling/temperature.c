#include "cooling/temperature.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    COOLING_TEMPERATURE_LOOKUP_COUNT = 34,
};

static const float resistance_lookup[COOLING_TEMPERATURE_LOOKUP_COUNT] = {
    336851.0f,
    256115.796875f,
    196435.203125f,
    151917.40625f,
    118422.203125f,
    93011.8984375f,
    73582.703125f,
    58614.6015625f,
    47000.0f,
    37925.30078125f,
    30788.099609375f,
    25139.099609375f,
    20640.80078125f,
    17037.80078125f,
    14135.7998046875f,
    11785.7998046875f,
    9872.900390625f,
    8308.099609375f,
    7021.89990234375f,
    5959.7001953125f,
    5078.7001953125f,
    4344.89990234375f,
    3731.0f,
    3215.5f,
    2781.0f,
    2413.199951171875f,
    2101.0f,
    1834.9000244140625f,
    1607.300048828125f,
    1412.199951171875f,
    1244.199951171875f,
    1099.300048828125f,
    973.79998779296875f,
    864.9000244140625f,
};

/**
 * @brief Converts thermistor resistance through the board's 5-degree calibration curve.
 *
 * Interpolates between adjacent resistance entries and returns the defined sentinel outside the
 * supported calibration range.
 *
 * @param[in] resistance_ohms Thermistor resistance in ohms.
 * @return Interpolated temperature in degrees Celsius, or the out-of-range sentinel.
 */
float cooling_temperature_from_resistance(float resistance_ohms) {
    if (resistance_ohms >= resistance_lookup[0]) {
        return -99.9f;
    }

    for (uint8_t upper = 1; upper < COOLING_TEMPERATURE_LOOKUP_COUNT; upper++) {
        if (resistance_lookup[upper] > resistance_ohms) {
            continue;
        }

        uint8_t lower = upper - 1;
        float fraction = (resistance_lookup[lower] - resistance_ohms) /
                         (resistance_lookup[lower] - resistance_lookup[upper]);
        return ((float)lower + fraction) * 5.0f - 15.0f;
    }

    return 999.9f;
}

/**
 * @brief Converts one 1,000-sample thermistor ADC total to an integer temperature.
 *
 * Converts the averaged ADC count through the 3.3-volt divider and calibrated resistance lookup,
 * then truncates the resulting temperature.
 *
 * @param[in] adc_total Sum of 1,000 12-bit ADC samples.
 * @return Truncated calibrated temperature in degrees Celsius.
 */
int16_t cooling_temperature_from_adc_total(uint32_t adc_total) {
    float average_adc = (float)adc_total / 1000.0f;
    float measured_voltage = average_adc * 3.3f * (1.0f / 4096.0f);
    float divider_ratio = 3.3f / measured_voltage - 1.0f;
    float resistance_ohms = 10000.0f / divider_ratio;
    return (int16_t)(int32_t)cooling_temperature_from_resistance(resistance_ohms);
}

void cooling_temperature_monitor_init(CoolingTemperatureMonitor *monitor) {
    *monitor = (CoolingTemperatureMonitor){0};
}

/**
 * @brief Accumulates both thermistor channels and publishes temperatures every 1,000 samples.
 *
 * Adds one sample per channel, converts a complete window, and clears both accumulators before the
 * next window begins.
 *
 * @param[in,out] monitor Temperature accumulators and latest converted values.
 * @param[in] primary_adc Primary 12-bit thermistor ADC sample.
 * @param[in] secondary_adc Secondary 12-bit thermistor ADC sample.
 * @return True when a complete sample window was converted and the accumulators were cleared.
 */
bool cooling_temperature_monitor_add(CoolingTemperatureMonitor *monitor, uint16_t primary_adc,
                                     uint16_t secondary_adc) {
    monitor->adc_totals[0] += primary_adc;
    monitor->adc_totals[1] += secondary_adc;
    monitor->sample_count++;
    if (monitor->sample_count != COOLING_TEMPERATURE_SAMPLE_COUNT) {
        return false;
    }

    for (uint8_t channel = 0; channel < COOLING_TEMPERATURE_CHANNEL_COUNT; channel++) {
        monitor->temperatures_c[channel] =
            cooling_temperature_from_adc_total(monitor->adc_totals[channel]);
        monitor->adc_totals[channel] = 0;
    }
    monitor->sample_count = 0;
    return true;
}
