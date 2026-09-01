#ifndef OPENTEC_BASE_PEDAL_AXIS_H
#define OPENTEC_BASE_PEDAL_AXIS_H

#include <stdint.h>

/**
 * @brief Stores one pedal axis calibration record.
 *
 * Endpoint learning flags control whether samples can extend the calibrated range.
 */
typedef struct {
    uint16_t minimum;        /**< Calibrated minimum raw sample. */
    uint16_t maximum;        /**< Calibrated maximum raw sample. */
    uint16_t lower_deadzone; /**< Raw counts removed from the lower endpoint. */
    uint16_t upper_deadzone; /**< Raw counts removed from the upper endpoint. */
    uint16_t output_scale;   /**< Maximum value of the scaled output. */
    uint8_t learn_minimum;   /**< Nonzero when samples may lower minimum. */
    uint8_t learn_maximum;   /**< Nonzero when samples may raise maximum. */
} PedalAxisCalibration;

/**
 * @brief Updates one calibrated pedal axis from a raw sample.
 *
 * Learns or clamps endpoints, applies deadzones, and scales the sample to output_scale.
 *
 * @param[in,out] calibration Axis calibration record to update.
 * @param[in] sample Raw pedal sample.
 * @return Scaled pedal-axis value.
 */
uint16_t pedal_axis_update(PedalAxisCalibration *calibration, uint16_t sample);

#endif
