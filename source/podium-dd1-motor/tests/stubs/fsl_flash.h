#ifndef MOTOR_TEST_FSL_FLASH_H
#define MOTOR_TEST_FSL_FLASH_H

#include <stdint.h>

#include "fsl_common.h"

typedef struct {
    uint32_t value;
} flash_config_t;

typedef enum {
    kFTFx_MarginValueNormal,
    kFTFx_MarginValueUser,
    kFTFx_MarginValueFactory,
} ftfx_margin_value_t;

enum {
    kStatus_FLASH_Success = kStatus_Success,
    kFLASH_ApiEraseKey = 0x6b65666bU,
};

status_t FLASH_Init(flash_config_t *config);
status_t FLASH_Erase(flash_config_t *config, uint32_t start, uint32_t length_in_bytes,
                     uint32_t key);
status_t FLASH_VerifyErase(flash_config_t *config, uint32_t start, uint32_t length_in_bytes,
                           ftfx_margin_value_t margin);
status_t FLASH_Program(flash_config_t *config, uint32_t start, uint8_t *source,
                       uint32_t length_in_bytes);
status_t FLASH_VerifyProgram(flash_config_t *config, uint32_t start, uint32_t length_in_bytes,
                             const uint8_t *expected_data, ftfx_margin_value_t margin,
                             uint32_t *failed_address, uint32_t *failed_data);

#endif
