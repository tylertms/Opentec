#include "analog/samples.h"

/**
 * @brief Decodes one ADC scan into logical wheel-base analog inputs.
 *
 * Maps scan slots zero through nine to both thermistors, the auxiliary axis, both two-axis shifter
 * ports, and the three pedal axes in acquisition order.
 *
 * @param[in] scan Ten-channel ADC scan to decode.
 * @param[out] samples Destination for the logical analog samples.
 */
void analog_samples_decode(const volatile uint16_t scan[ANALOG_SCAN_SAMPLE_COUNT],
                           AnalogSamples *samples) {
    samples->primary_thermistor = scan[0];
    samples->secondary_thermistor = scan[1];
    samples->auxiliary_axis = scan[2];
    samples->primary_shifter_y = scan[3];
    samples->primary_shifter_x = scan[4];
    samples->secondary_shifter_y = scan[5];
    samples->secondary_shifter_x = scan[6];
    samples->pedal_axes[0] = scan[7];
    samples->pedal_axes[1] = scan[8];
    samples->pedal_axes[2] = scan[9];
}
