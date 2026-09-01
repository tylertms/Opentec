#include "platform/storage.h"

#include <string.h>

#include "fsl_flash.h"

/**
 * @brief Flash region constants for the persisted calibration record.
 */
enum {
    /** @brief First flash address reserved for calibration storage. */
    MOTOR_CALIBRATION_FLASH_START = 0x0001d800U, /**< Calibration-storage base address. */
    /** @brief First flash address after the calibration-storage region. */
    MOTOR_CALIBRATION_FLASH_END = 0x00020000U, /**< Exclusive calibration-storage end address. */
    /** @brief Flash sector size used by the calibration-storage region. */
    MOTOR_CALIBRATION_FLASH_SECTOR_SIZE = 0x800U, /**< Calibration-storage sector size in bytes. */
};

/** @brief NXP flash-driver configuration shared by calibration operations. */
static flash_config_t motor_calibration_flash;

status_t motor_calibration_storage_initialize(void) { return FLASH_Init(&motor_calibration_flash); }

bool motor_calibration_storage_load(MotorEncoderCalibrationRecord *record) {
    memcpy(record, (const void *)MOTOR_CALIBRATION_FLASH_START, sizeof(*record));
    if (motor_encoder_calibration_record_is_valid(record)) {
        return true;
    }

    memset(record, 0, sizeof(*record));
    return false;
}

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
                                       MOTOR_CALIBRATION_FLASH_SECTOR_SIZE, margin);
            if (status != kStatus_FLASH_Success) {
                return status;
            }
        }
    }

    return kStatus_FLASH_Success;
}

status_t motor_calibration_storage_program(const MotorEncoderCalibrationRecord *record) {
    if (!motor_encoder_calibration_record_is_valid(record)) {
        return kStatus_Fail;
    }

    const uint8_t *bytes = (const uint8_t *)record;
    status_t status = FLASH_Program(&motor_calibration_flash, MOTOR_CALIBRATION_FLASH_START + 4U,
                                    (uint8_t *)(uintptr_t)(bytes + 4U), sizeof(*record) - 4U);
    if (status != kStatus_FLASH_Success) {
        return status;
    }

    uint32_t failed_address;
    uint32_t failed_data;
    status = FLASH_VerifyProgram(&motor_calibration_flash, MOTOR_CALIBRATION_FLASH_START + 4U,
                                 sizeof(*record) - 4U, bytes + 4U, kFTFx_MarginValueUser,
                                 &failed_address, &failed_data);
    if (status != kStatus_FLASH_Success) {
        return status;
    }

    status = FLASH_Program(&motor_calibration_flash, MOTOR_CALIBRATION_FLASH_START,
                           (uint8_t *)(uintptr_t)bytes, 4U);
    if (status != kStatus_FLASH_Success) {
        return status;
    }

    return FLASH_VerifyProgram(&motor_calibration_flash, MOTOR_CALIBRATION_FLASH_START,
                               sizeof(*record), (const uint8_t *)record, kFTFx_MarginValueUser,
                               &failed_address, &failed_data);
}
