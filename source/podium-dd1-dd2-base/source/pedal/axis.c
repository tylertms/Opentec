#include "pedal/axis.h"

#include <stdint.h>

/**
 * @brief Analog axis sample limits.
 */
enum { PEDAL_AXIS_SAMPLE_MAXIMUM = 0x0ffe /**< Maximum oriented twelve-bit axis sample. */ };

/**
 * @brief Clamps or learns one pedal sample and scales it across the calibrated travel.
 *
 * Learns enabled endpoints, clamps disabled endpoints, applies the lower and upper margins, and
 * scales the remaining travel to the configured output range. Insufficient travel returns zero.
 *
 * @param[in,out] calibration Axis limits, margins, scale, and learning controls.
 * @param[in] sample Raw 12-bit pedal sample.
 * @return Scaled axis value limited by the configured output scale.
 */
uint16_t pedal_axis_update(PedalAxisCalibration *calibration, uint16_t sample) {
    if (sample > calibration->maximum) {
        if (calibration->learn_maximum != 0) {
            calibration->maximum = sample;
        } else {
            sample = calibration->maximum;
        }
    } else if (sample < calibration->minimum) {
        if (calibration->learn_minimum != 0) {
            calibration->minimum = sample;
        } else {
            sample = calibration->minimum;
        }
    }

    uint16_t maximum_start = (uint16_t)(PEDAL_AXIS_SAMPLE_MAXIMUM - calibration->lower_deadzone);
    uint16_t start = calibration->minimum < maximum_start
                         ? (uint16_t)(calibration->minimum + calibration->lower_deadzone)
                         : maximum_start;
    uint16_t end = calibration->maximum >= calibration->upper_deadzone
                       ? (uint16_t)(calibration->maximum - calibration->upper_deadzone)
                       : calibration->upper_deadzone;
    if (end < start) {
        return 0;
    }
    uint16_t available = (uint16_t)(end - start);
    uint16_t used = (uint16_t)(calibration->lower_deadzone + calibration->upper_deadzone);
    if (available < used) {
        return 0;
    }

    uint16_t offset = sample >= start ? (uint16_t)(sample - start) : 0;
    uint32_t scaled = (uint32_t)offset * calibration->output_scale / available;
    return scaled > calibration->output_scale ? calibration->output_scale : (uint16_t)scaled;
}
