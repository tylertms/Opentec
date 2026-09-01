#ifndef OPENTEC_BASE_WHEEL_POSITION_H
#define OPENTEC_BASE_WHEEL_POSITION_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Wheel-position scale and supported sample limits. */
enum {
    WHEEL_POSITION_COUNTS_PER_REVOLUTION = 23680, /**< Counts in one motor revolution. */
    WHEEL_POSITION_SAMPLE_LIMIT = 82880,          /**< Maximum one-sided position sample. */
};

/** @brief Retained absolute wheel center reference. */
typedef struct {
    int32_t center;  /**< Signed absolute center sample. */
    bool calibrated; /**< Whether the center reference is available. */
} WheelPositionReference;

/** @brief Calibration values used to transform wheel position samples. */
typedef struct {
    int32_t center;    /**< Signed absolute center sample. */
    uint32_t travel;   /**< One-sided travel limit in position counts. */
    uint32_t deadband; /**< Center deadband in position counts. */
} WheelPositionCalibration;

/** @brief Filter state for the wheel velocity estimator. */
typedef struct {
    float filtered_position; /**< Predicted and corrected position state. */
    float filtered_velocity; /**< Predicted and corrected velocity state. */
    uint32_t next_update_ms; /**< Earliest time for the next filter update. */
    int32_t scaled_velocity; /**< Last velocity converted to device units. */
} WheelVelocityEstimator;

/**
 * @brief Centers an absolute wheel-position sample.
 *
 * Subtracts the retained center and saturates the signed displacement to the supported sample
 * range.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] center Retained absolute center reference.
 * @return Signed displacement from center, constrained to the supported range.
 */
int32_t wheel_position_center(int32_t sample, int32_t center);

/**
 * @brief Applies wheel centering and steering deadband.
 *
 * Removes the configured deadband from the displacement magnitude and returns zero inside the
 * deadband.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Center, travel, and deadband calibration.
 * @return Centered displacement with the deadband removed.
 */
int32_t wheel_position_filter(int32_t sample, const WheelPositionCalibration *calibration);

/**
 * @brief Converts a wheel-position sample to a signed HID axis.
 *
 * Applies centering and deadband, scales each direction to its signed sixteen-bit limit, and
 * saturates at the configured travel.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Center, travel, and deadband calibration.
 * @return Signed sixteen-bit steering axis.
 */
int16_t wheel_position_axis(int32_t sample, const WheelPositionCalibration *calibration);

/**
 * @brief Converts a wheel-position sample to an unsigned HID axis.
 *
 * Offsets the signed axis by 32768 so the center is 32768 and the endpoints span the unsigned
 * sixteen-bit range.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Center, travel, and deadband calibration.
 * @return Unsigned sixteen-bit steering axis.
 */
uint16_t wheel_position_hid_axis(int32_t sample, const WheelPositionCalibration *calibration);

/**
 * @brief Converts a wheel-position sample to a display rotation angle.
 *
 * Converts the calibrated axis to hundredths of a degree and folds the result into the signed
 * range from -18000 through 18000.
 *
 * @param[in] sample Current absolute wheel-position sample.
 * @param[in] calibration Center, travel, and deadband calibration.
 * @return Signed display angle in hundredths of a degree.
 */
int16_t wheel_position_display_rotation(int32_t sample,
                                        const WheelPositionCalibration *calibration);

/**
 * @brief Clears the retained wheel center reference.
 *
 * Resets the center sample and marks the reference unavailable.
 *
 * @param[out] reference Center reference to reset.
 */
void wheel_position_reference_reset(WheelPositionReference *reference);

/**
 * @brief Captures an absolute wheel sample as the center reference.
 *
 * Reduces the sample by the motor-controller modulus, stores the normalized center, and marks the
 * reference calibrated.
 *
 * @param[in,out] reference Center reference to update.
 * @param[in] sample Absolute wheel-position sample.
 * @param[in] modulus Motor-controller position modulus.
 * @return True when the stored center or calibrated state changed; otherwise false.
 */
bool wheel_position_reference_capture(WheelPositionReference *reference, int32_t sample,
                                      uint32_t modulus);

/**
 * @brief Converts a configured wheel range to a one-sided travel limit.
 *
 * Uses the wheel's revolution scale and caps the result at the supported sample limit.
 *
 * @param[in] rotation_degrees Configured lock-to-lock wheel range in degrees.
 * @return One-sided travel limit in wheel counts.
 */
uint32_t wheel_position_travel_from_degrees(uint16_t rotation_degrees);

/**
 * @brief Builds wheel-position calibration from retained settings.
 *
 * Uses zero travel until the center reference is calibrated and converts the deadzone from ten-
 * count steps to position counts.
 *
 * @param[in] reference Retained wheel center reference.
 * @param[in] rotation_degrees Configured lock-to-lock wheel range in degrees.
 * @param[in] deadzone Configured steering deadzone in ten-count steps.
 * @return Complete position calibration.
 */
WheelPositionCalibration wheel_position_calibration_build(const WheelPositionReference *reference,
                                                          uint16_t rotation_degrees,
                                                          uint8_t deadzone);

/**
 * @brief Clears wheel velocity-estimator state.
 *
 * Resets filtered position, filtered velocity, update timing, and the scaled output.
 *
 * @param[out] estimator Velocity estimator to initialize.
 */
void wheel_velocity_reset(WheelVelocityEstimator *estimator);

/**
 * @brief Updates the filtered wheel velocity.
 *
 * Runs the estimator no more than once per millisecond and returns the most recent scaled output
 * when called before the next update deadline.
 *
 * @param[in,out] estimator Velocity estimator state.
 * @param[in] position Current centered wheel position.
 * @param[in] time_ms Current monotonic time in milliseconds.
 * @return Latest signed and scaled wheel velocity.
 */
int32_t wheel_velocity_update(WheelVelocityEstimator *estimator, int32_t position,
                              uint32_t time_ms);

#endif
