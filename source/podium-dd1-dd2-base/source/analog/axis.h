#ifndef OPENTEC_BASE_ANALOG_AXIS_H
#define OPENTEC_BASE_ANALOG_AXIS_H

#include <stdint.h>

/**
 * @brief Number of entries in the analog-axis moving-average ring.
 *
 * The filter retains up to five accepted samples when calculating its current output.
 */
enum {
    ANALOG_AXIS_FILTER_SAMPLES = 5, /**< Number of samples retained by AnalogAxisFilter. */
};

/**
 * @brief State for a deadband-gated moving-average filter.
 *
 * The state stores the running sum and ring position so each accepted sample can replace one old
 * sample without rescanning the ring.
 */
typedef struct {
    uint32_t total;                               /**< Running sum of the populated ring entries. */
    uint16_t samples[ANALOG_AXIS_FILTER_SAMPLES]; /**< Ring of accepted samples. */
    uint16_t value;                               /**< Current filtered value. */
    uint8_t count;                                /**< Number of populated entries in samples. */
    uint8_t next_sample; /**< Index of the ring entry replaced by the next accepted sample. */
} AnalogAxisFilter;

/**
 * @brief Calibration settings for an unsigned, one-direction axis.
 *
 * Samples at minimum and maximum map to the ends of the unsigned output range; inverted reverses
 * that mapping.
 */
typedef struct {
    uint16_t minimum; /**< Input sample mapped to zero. */
    uint16_t maximum; /**< Input sample mapped to UINT16_MAX. */
    uint8_t inverted; /**< Nonzero to reverse the output direction. */
} AnalogUnipolarCalibration;

/**
 * @brief Calibration settings for a centered, signed axis.
 *
 * Samples at minimum, center, and maximum map to the negative endpoint, zero, and positive
 * endpoint; inverted reverses that signed mapping.
 */
typedef struct {
    uint16_t minimum; /**< Input sample mapped to the negative endpoint. */
    uint16_t center;  /**< Input sample mapped to zero. */
    uint16_t maximum; /**< Input sample mapped to the positive endpoint. */
    uint8_t inverted; /**< Nonzero to reverse the signed output direction. */
} AnalogBipolarCalibration;

/**
 * @brief Applies unipolar endpoint calibration to one analog sample.
 *
 * Clamps the sample to the configured endpoints, scales it to the unsigned 16-bit range, and
 * reverses the result when calibration->inverted is nonzero.
 *
 * @param[in] sample Raw analog sample to calibrate.
 * @param[in] calibration Minimum, maximum, and direction settings.
 * @return Calibrated unsigned 16-bit value, or zero for an invalid endpoint range.
 */
uint16_t analog_axis_unipolar(uint16_t sample, const AnalogUnipolarCalibration *calibration);

/**
 * @brief Applies centered bipolar calibration to one analog sample.
 *
 * Clamps the sample to the configured endpoints, scales each side of center to the signed 16-bit
 * range, and reverses the result when calibration->inverted is nonzero.
 *
 * @param[in] sample Raw analog sample to calibrate.
 * @param[in] calibration Minimum, center, maximum, and direction settings.
 * @return Calibrated signed 16-bit value, or zero for an invalid endpoint range.
 */
int16_t analog_axis_bipolar(uint16_t sample, const AnalogBipolarCalibration *calibration);

/**
 * @brief Applies deadband-gated moving-average filtering to one sample.
 *
 * Retains the previous value when the sample is within the inclusive deadband; otherwise replaces
 * the oldest ring entry and returns the average of all populated entries.
 *
 * @param[in,out] filter Moving-average state to update.
 * @param[in] sample New raw sample.
 * @param[in] deadband Maximum difference retained without accepting the sample.
 * @return Current filtered value.
 */
uint16_t analog_axis_filter(AnalogAxisFilter *filter, uint16_t sample, uint16_t deadband);

/**
 * @brief Scales one sample across an unsigned 16-bit output range.
 *
 * Clamps the sample to the supplied endpoints and applies linear integer scaling; an invalid or
 * empty endpoint range produces zero.
 *
 * @param[in] sample Sample to scale.
 * @param[in] minimum Input value mapped to zero.
 * @param[in] maximum Input value mapped to UINT16_MAX.
 * @return Scaled unsigned 16-bit value.
 */
uint16_t analog_axis_scale(uint16_t sample, uint16_t minimum, uint16_t maximum);

#endif
