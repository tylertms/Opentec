#include "analog/axis.h"

#include <stdint.h>

/**
 * @brief Clamps one sample to an inclusive endpoint range.
 *
 * Returns the nearest endpoint when the sample lies outside the range.
 *
 * @param[in] sample Sample to clamp.
 * @param[in] minimum Inclusive minimum endpoint.
 * @param[in] maximum Inclusive maximum endpoint.
 * @return Clamped sample.
 */
static uint16_t clamp_sample(uint16_t sample, uint16_t minimum, uint16_t maximum) {
    if (sample < minimum) {
        return minimum;
    }
    if (sample > maximum) {
        return maximum;
    }
    return sample;
}

/**
 * @brief Applies deadband-gated five-sample moving-average filtering.
 *
 * Retains the previous value while the new sample remains inside the inclusive deadband. Accepted
 * samples replace the oldest ring entry and are averaged over the populated sample count.
 *
 * @param[in,out] filter Moving-average state to update.
 * @param[in] sample New sample.
 * @param[in] deadband Maximum retained difference from the current filtered value.
 * @return Current filtered value.
 */
uint16_t analog_axis_filter(AnalogAxisFilter *filter, uint16_t sample, uint16_t deadband) {
    uint16_t difference = sample > filter->value ? sample - filter->value : filter->value - sample;
    if (difference <= deadband) {
        return filter->value;
    }

    filter->total -= filter->samples[filter->next_sample];
    filter->samples[filter->next_sample] = sample;
    filter->total += sample;

    if (filter->count < ANALOG_AXIS_FILTER_SAMPLES) {
        filter->count++;
    }

    filter->next_sample++;
    if (filter->next_sample == ANALOG_AXIS_FILTER_SAMPLES) {
        filter->next_sample = 0;
    }

    filter->value = (uint16_t)(filter->total / filter->count);
    return filter->value;
}

/**
 * @brief Scales one sample across an unsigned 16-bit output range.
 *
 * Clamps the sample to the supplied endpoints and applies linear integer scaling. Invalid or empty
 * endpoint ranges produce zero.
 *
 * @param[in] sample Sample to scale.
 * @param[in] minimum Input value mapped to zero.
 * @param[in] maximum Input value mapped to 65535.
 * @return Scaled unsigned 16-bit value.
 */
uint16_t analog_axis_scale(uint16_t sample, uint16_t minimum, uint16_t maximum) {
    if (minimum >= maximum) {
        return 0;
    }

    sample = clamp_sample(sample, minimum, maximum);
    return (uint16_t)((uint32_t)(sample - minimum) * UINT16_MAX / (maximum - minimum));
}

/**
 * @brief Applies unipolar endpoint calibration to one analog sample.
 *
 * Scales the sample to the unsigned 16-bit range and optionally reverses its direction.
 *
 * @param[in] sample Sample to calibrate.
 * @param[in] calibration Minimum, maximum, and direction settings.
 * @return Calibrated unsigned 16-bit value.
 */
uint16_t analog_axis_unipolar(uint16_t sample, const AnalogUnipolarCalibration *calibration) {
    if (calibration->minimum >= calibration->maximum) {
        return 0;
    }

    uint16_t value = analog_axis_scale(sample, calibration->minimum, calibration->maximum);
    return calibration->inverted == 0 ? value : (uint16_t)(UINT16_MAX - value);
}

/**
 * @brief Applies centered bipolar calibration to one analog sample.
 *
 * Scales each side of the center independently to preserve both endpoint ranges and optionally
 * reverses the signed output direction.
 *
 * @param[in] sample Sample to calibrate.
 * @param[in] calibration Minimum, center, maximum, and direction settings.
 * @return Calibrated signed 16-bit value.
 */
int16_t analog_axis_bipolar(uint16_t sample, const AnalogBipolarCalibration *calibration) {
    if (calibration->minimum >= calibration->center ||
        calibration->center >= calibration->maximum) {
        return 0;
    }

    sample = clamp_sample(sample, calibration->minimum, calibration->maximum);
    int32_t value;
    if (sample < calibration->center) {
        value = -(int32_t)((uint32_t)(calibration->center - sample) * 32768u /
                           (calibration->center - calibration->minimum));
    } else {
        value = (int32_t)((uint32_t)(sample - calibration->center) * 32767u /
                          (calibration->maximum - calibration->center));
    }
    if (calibration->inverted == 0) {
        return (int16_t)value;
    }
    return value == INT16_MIN ? INT16_MAX : (int16_t)-value;
}
