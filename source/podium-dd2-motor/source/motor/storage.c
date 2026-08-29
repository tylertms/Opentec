#include "motor/storage.h"

#include <string.h>

#include "fsl_flash.h"

enum {
    MOTOR_CALIBRATION_FLASH_START = 0x0001d800U,
    MOTOR_CALIBRATION_FLASH_END = 0x00020000U,
    MOTOR_CALIBRATION_FLASH_SECTOR_SIZE = 0x800U,
    MOTOR_CALIBRATION_FLASH_VERIFY_SIZE = 0x100U,
};

static flash_config_t motor_calibration_flash;

/**
 * @brief Initializes the official NXP flash driver used by calibration storage.
 *
 * The shared flash configuration is prepared before any calibration record operation.
 *
 * @return NXP SDK flash status.
 */
status_t motor_calibration_storage_initialize(void) { return FLASH_Init(&motor_calibration_flash); }

/**
 * @brief Loads and validates the official encoder correction record from flash.
 *
 * Invalid record headers clear both directional correction tables before control continues.
 *
 * @param record Destination record in RAM.
 * @return True when the persisted magic and version are valid.
 */
bool motor_calibration_storage_load(MotorEncoderCalibrationRecord *record) {
    memcpy(record, (const void *)MOTOR_CALIBRATION_FLASH_START, sizeof(*record));
    if (motor_encoder_calibration_record_is_valid(record)) {
        return true;
    }

    memset(record->forward, 0, sizeof(record->forward));
    memset(record->reverse, 0, sizeof(record->reverse));
    return false;
}

/**
 * @brief Erases and verifies the official encoder calibration flash range.
 *
 * Every covered sector is erased and checked at both supported verification margins.
 *
 * @return NXP SDK flash status.
 */
status_t motor_calibration_storage_erase(void) {
    for (uint32_t address = MOTOR_CALIBRATION_FLASH_START;
         address + MOTOR_CALIBRATION_FLASH_SECTOR_SIZE <= MOTOR_CALIBRATION_FLASH_END;
         address += MOTOR_CALIBRATION_FLASH_SECTOR_SIZE) {
        status_t status = FLASH_Erase(&motor_calibration_flash, address,
                                      MOTOR_CALIBRATION_FLASH_SECTOR_SIZE, kFLASH_ApiEraseKey);
        if (status != kStatus_FLASH_Success) {
            return status;
        }

        for (ftfx_margin_value_t margin = kFTFx_MarginValueNormal; margin <= kFTFx_MarginValueUser;
             ++margin) {
            status = FLASH_VerifyErase(&motor_calibration_flash, address,
                                       MOTOR_CALIBRATION_FLASH_VERIFY_SIZE, margin);
            if (status != kStatus_FLASH_Success) {
                return status;
            }
        }
    }

    return kStatus_FLASH_Success;
}

/**
 * @brief Programs and verifies one official encoder correction record.
 *
 * The complete record is written at the calibration base and checked at the user margin.
 *
 * @param record Completed calibration record in RAM.
 * @return NXP SDK flash status.
 */
status_t motor_calibration_storage_program(const MotorEncoderCalibrationRecord *record) {
    status_t status = FLASH_Program(&motor_calibration_flash, MOTOR_CALIBRATION_FLASH_START,
                                    (uint8_t *)(uintptr_t)record, sizeof(*record));
    if (status != kStatus_FLASH_Success) {
        return status;
    }

    uint32_t failed_address;
    uint32_t failed_data;
    return FLASH_VerifyProgram(&motor_calibration_flash, MOTOR_CALIBRATION_FLASH_START,
                               sizeof(*record), (const uint8_t *)record, kFTFx_MarginValueUser,
                               &failed_address, &failed_data);
}
