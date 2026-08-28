#ifndef OPENTEC_BASE_COOLING_TEMPERATURE_H
#define OPENTEC_BASE_COOLING_TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    COOLING_TEMPERATURE_CHANNEL_COUNT = 2,
    COOLING_TEMPERATURE_SAMPLE_COUNT = 1000,
};

typedef struct {
    uint32_t adc_totals[COOLING_TEMPERATURE_CHANNEL_COUNT];
    uint16_t sample_count;
    int16_t temperatures_c[COOLING_TEMPERATURE_CHANNEL_COUNT];
} CoolingTemperatureMonitor;

void cooling_temperature_monitor_init(CoolingTemperatureMonitor *monitor);
bool cooling_temperature_monitor_add(CoolingTemperatureMonitor *monitor, uint16_t primary_adc,
                                     uint16_t secondary_adc);
float cooling_temperature_from_resistance(float resistance_ohms);
int16_t cooling_temperature_from_adc_total(uint32_t adc_total);

#endif
