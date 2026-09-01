#ifndef OPENTEC_MOTOR_PROFILE_H
#define OPENTEC_MOTOR_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Holds the board-specific motor timing, scaling, and sensing values.
 */
typedef struct {
    uint16_t encoder_period;          /**< Encoder counts per revolution used by calibration thresholds. */
    uint32_t encoder_modulus;         /**< FTM2 MOD value used for quadrature-counter wrapping. */
    uint32_t position_limit;          /**< Exclusive signed extended-position safety limit. */
    uint32_t position_scale;          /**< Fixed-point scale from encoder count to electrical angle. */
    uint32_t velocity_scale;          /**< Fixed-point scale from encoder motion to velocity. */
    uint32_t secondary_scale;         /**< Secondary motion scale used by retained-position friction. */
    uint16_t correction_table_length; /**< Number of entries in each encoder correction table. */
    uint8_t adc_auxiliary_channel;    /**< ADC0 channel used for the motor DC-bus voltage sample. */
} MotorHardwareProfile;

/**
 * @brief Selects the complete motor hardware profile for a board variant.
 *
 * The strap-selected variant supplies encoder constants, fixed-point scales, correction-table
 * length, and the ADC channel used for the DC-bus voltage sample.
 *
 * @param[in] alternate_hardware True when the alternate board profile is selected.
 * @return Complete motor hardware profile.
 */
MotorHardwareProfile motor_hardware_profile_select(bool alternate_hardware);

#endif
