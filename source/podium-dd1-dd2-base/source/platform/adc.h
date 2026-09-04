#ifndef OPENTEC_BASE_PLATFORM_ADC_H
#define OPENTEC_BASE_PLATFORM_ADC_H

#include <stdbool.h>

#include "analog/samples.h"

/**
 * @brief Initializes continuous analog sampling.
 *
 * Configures the ADC scan and DMA ping-pong buffers used by the platform analog inputs.
 */
void platform_adc_init(void);

/**
 * @brief Takes the newest completed analog sample set.
 *
 * Decodes a completed DMA buffer into logical analog input values.
 *
 * @param[out] samples Destination for decoded analog samples.
 * @return True when a completed scan was available; otherwise false.
 */
bool platform_adc_read(AnalogSamples *samples);

/**
 * @brief Reads the newest raw auxiliary-axis ADC sample.
 *
 * Returns the sample from the DMA buffer most recently completed by the ADC, without consuming
 * the foreground-ready indication used by platform_adc_read().
 *
 * @return Raw 12-bit auxiliary-axis ADC sample.
 */
uint16_t platform_adc_latest_auxiliary_axis_sample(void);

/**
 * @brief Averages one shifter longitudinal analog input.
 *
 * Accumulates reads from the newest completed scan buffer for the selected primary or secondary
 * shifter channel.
 *
 * @param[in] secondary True to select the secondary shifter; false for the primary shifter.
 * @return Arithmetic mean of the selected analog input.
 */
uint16_t platform_adc_average_shifter_y(bool secondary);

#endif
