#ifndef OPENTEC_BASE_ANALOG_SAMPLES_H
#define OPENTEC_BASE_ANALOG_SAMPLES_H

#include <stdint.h>

/**
 * @brief Number of ADC entries in one analog scan.
 *
 * The scan contains the two thermistors, auxiliary axis, shifter inputs, and three pedal inputs
 * consumed by analog_samples_decode().
 */
enum { ANALOG_SCAN_SAMPLE_COUNT = 10 /**< Number of ADC entries in one scan. */ };

/**
 * @brief Logical analog inputs decoded from one ADC scan.
 *
 * The members retain the acquisition order used by the platform ADC driver while giving callers
 * named access to each wheel-base input.
 */
typedef struct {
    uint16_t primary_thermistor;   /**< Primary motor thermistor ADC sample. */
    uint16_t secondary_thermistor; /**< Secondary motor thermistor ADC sample. */
    uint16_t auxiliary_axis;       /**< Auxiliary axis ADC sample. */
    uint16_t primary_shifter_y;    /**< Primary shifter Y-axis ADC sample. */
    uint16_t primary_shifter_x;    /**< Primary shifter X-axis ADC sample. */
    uint16_t secondary_shifter_y;  /**< Secondary shifter Y-axis ADC sample. */
    uint16_t secondary_shifter_x;  /**< Secondary shifter X-axis ADC sample. */
    uint16_t pedal_axes[3];        /**< ADC samples for the three pedal axes, in pedal order. */
} AnalogSamples;

/**
 * @brief Decodes one ADC scan into named wheel-base analog inputs.
 *
 * Copies each acquisition slot into the corresponding member of the destination structure without
 * changing the ADC values.
 *
 * @param[in] scan Ten-channel ADC scan to decode.
 * @param[out] samples Destination for the logical analog samples.
 */
void analog_samples_decode(const volatile uint16_t scan[ANALOG_SCAN_SAMPLE_COUNT],
                           AnalogSamples *samples);

#endif
