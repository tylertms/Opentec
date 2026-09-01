#ifndef OPENTEC_MOTOR_STORAGE_H
#define OPENTEC_MOTOR_STORAGE_H

#include <stdbool.h>

#include "fsl_common.h"
#include "motor/encoder_calibration.h"

/**
 * @brief Initializes the flash driver used by calibration storage.
 *
 * The shared flash configuration is prepared before any calibration record operation.
 *
 * @return NXP SDK flash status.
 */
status_t motor_calibration_storage_initialize(void);

/**
 * @brief Loads and validates the persisted encoder calibration record.
 *
 * An invalid record is cleared before control continues.
 *
 * @param[out] record Destination record in RAM.
 * @return True when the persisted record header and payload are valid.
 */
bool motor_calibration_storage_load(MotorEncoderCalibrationRecord *record);

/**
 * @brief Erases and verifies the flash range reserved for calibration storage.
 *
 * Every covered sector is erased and checked at the supported verification margins.
 *
 * @return NXP SDK flash status.
 */
status_t motor_calibration_storage_erase(void);

/**
 * @brief Programs and verifies one encoder calibration record.
 *
 * The record payload is verified before its magic is committed, then the complete record is
 * verified at the flash user margin.
 *
 * @param[in] record Completed calibration record in RAM.
 * @return NXP SDK flash status.
 */
status_t motor_calibration_storage_program(const MotorEncoderCalibrationRecord *record);

#endif
