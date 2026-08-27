#ifndef OPENTEC_BASE_PLATFORM_ADC_H
#define OPENTEC_BASE_PLATFORM_ADC_H

#include <stdbool.h>

#include "analog/samples.h"

void platform_adc_init(void);
bool platform_adc_read(AnalogSamples *samples);

#endif
