#include "platform/storage.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fsl_flash.h"

enum {
    FLASH_START = 0x0001d800U,
    FLASH_END = 0x00020000U,
    FLASH_SECTOR_SIZE = 0x800U,
    MAX_CALLS = 16,
};

typedef struct {
    uint32_t start;
    uint32_t length;
    const uint8_t *data;
    ftfx_margin_value_t margin;
} FlashCall;

static FlashCall program_calls[MAX_CALLS];
static FlashCall verify_program_calls[MAX_CALLS];
static FlashCall erase_calls[MAX_CALLS];
static FlashCall verify_erase_calls[MAX_CALLS];
static status_t program_results[MAX_CALLS];
static status_t verify_program_results[MAX_CALLS];
static size_t program_count;
static size_t verify_program_count;
static size_t erase_count;
static size_t verify_erase_count;
static bool record_valid;

static void reset_fixture(void) {
    memset(program_calls, 0, sizeof(program_calls));
    memset(verify_program_calls, 0, sizeof(verify_program_calls));
    memset(erase_calls, 0, sizeof(erase_calls));
    memset(verify_erase_calls, 0, sizeof(verify_erase_calls));
    memset(program_results, 0, sizeof(program_results));
    memset(verify_program_results, 0, sizeof(verify_program_results));
    program_count = 0U;
    verify_program_count = 0U;
    erase_count = 0U;
    verify_erase_count = 0U;
    record_valid = true;
}

bool motor_encoder_calibration_record_is_valid(const MotorEncoderCalibrationRecord *record) {
    (void)record;
    return record_valid;
}

status_t FLASH_Init(flash_config_t *config) {
    (void)config;
    return kStatus_FLASH_Success;
}

status_t FLASH_Erase(flash_config_t *config, uint32_t start, uint32_t length_in_bytes,
                     uint32_t key) {
    (void)config;
    assert(key == kFLASH_ApiEraseKey);
    assert(erase_count < MAX_CALLS);
    erase_calls[erase_count++] = (FlashCall){.start = start, .length = length_in_bytes};
    return kStatus_FLASH_Success;
}

status_t FLASH_VerifyErase(flash_config_t *config, uint32_t start, uint32_t length_in_bytes,
                           ftfx_margin_value_t margin) {
    (void)config;
    assert(verify_erase_count < MAX_CALLS);
    verify_erase_calls[verify_erase_count++] =
        (FlashCall){.start = start, .length = length_in_bytes, .margin = margin};
    return kStatus_FLASH_Success;
}

status_t FLASH_Program(flash_config_t *config, uint32_t start, uint8_t *source,
                       uint32_t length_in_bytes) {
    (void)config;
    assert(program_count < MAX_CALLS);
    size_t index = program_count++;
    program_calls[index] = (FlashCall){.start = start, .length = length_in_bytes, .data = source};
    return program_results[index];
}

status_t FLASH_VerifyProgram(flash_config_t *config, uint32_t start, uint32_t length_in_bytes,
                             const uint8_t *expected_data, ftfx_margin_value_t margin,
                             uint32_t *failed_address, uint32_t *failed_data) {
    (void)config;
    assert(verify_program_count < MAX_CALLS);
    *failed_address = 0U;
    *failed_data = 0U;
    size_t index = verify_program_count++;
    verify_program_calls[index] = (FlashCall){
        .start = start,
        .length = length_in_bytes,
        .data = expected_data,
        .margin = margin,
    };
    return verify_program_results[index];
}

static void test_invalid_record_is_not_programmed(void) {
    MotorEncoderCalibrationRecord record = {0};
    reset_fixture();
    record_valid = false;
    assert(motor_calibration_storage_program(&record) == kStatus_Fail);
    assert(program_count == 0U);
    assert(verify_program_count == 0U);
}

static void test_payload_verification_precedes_magic(void) {
    MotorEncoderCalibrationRecord record = {0};
    reset_fixture();
    verify_program_results[0] = kStatus_Fail;

    assert(motor_calibration_storage_program(&record) == kStatus_Fail);
    assert(program_count == 1U);
    assert(verify_program_count == 1U);
    assert(program_calls[0].start == FLASH_START + 4U);
    assert(program_calls[0].length == sizeof(record) - 4U);
    assert(program_calls[0].data == (const uint8_t *)&record + 4U);
    assert(verify_program_calls[0].start == FLASH_START + 4U);
    assert(verify_program_calls[0].length == sizeof(record) - 4U);
    assert(verify_program_calls[0].margin == kFTFx_MarginValueUser);
}

static void test_magic_failure_stops_final_verification(void) {
    MotorEncoderCalibrationRecord record = {0};
    reset_fixture();
    program_results[1] = kStatus_Fail;

    assert(motor_calibration_storage_program(&record) == kStatus_Fail);
    assert(program_count == 2U);
    assert(verify_program_count == 1U);
    assert(program_calls[1].start == FLASH_START);
    assert(program_calls[1].length == 4U);
    assert(program_calls[1].data == (const uint8_t *)&record);
}

static void test_success_verifies_payload_and_committed_record(void) {
    MotorEncoderCalibrationRecord record = {0};
    reset_fixture();

    assert(motor_calibration_storage_program(&record) == kStatus_FLASH_Success);
    assert(program_count == 2U);
    assert(verify_program_count == 2U);
    assert(verify_program_calls[1].start == FLASH_START);
    assert(verify_program_calls[1].length == sizeof(record));
    assert(verify_program_calls[1].data == (const uint8_t *)&record);
    assert(verify_program_calls[1].margin == kFTFx_MarginValueUser);
}

static void test_complete_sectors_are_verified_at_both_margins(void) {
    reset_fixture();
    assert(motor_calibration_storage_erase() == kStatus_FLASH_Success);

    size_t sector_count = (FLASH_END - FLASH_START) / FLASH_SECTOR_SIZE;
    assert(erase_count == sector_count);
    assert(verify_erase_count == sector_count * 2U);
    for (size_t sector = 0U; sector < sector_count; ++sector) {
        uint32_t start = FLASH_START + (uint32_t)sector * FLASH_SECTOR_SIZE;
        assert(erase_calls[sector].start == start);
        assert(erase_calls[sector].length == FLASH_SECTOR_SIZE);
        assert(verify_erase_calls[sector * 2U].start == start);
        assert(verify_erase_calls[sector * 2U].length == FLASH_SECTOR_SIZE);
        assert(verify_erase_calls[sector * 2U].margin == kFTFx_MarginValueNormal);
        assert(verify_erase_calls[sector * 2U + 1U].start == start);
        assert(verify_erase_calls[sector * 2U + 1U].length == FLASH_SECTOR_SIZE);
        assert(verify_erase_calls[sector * 2U + 1U].margin == kFTFx_MarginValueUser);
    }
}

int main(void) {
    test_invalid_record_is_not_programmed();
    test_payload_verification_precedes_magic();
    test_magic_failure_stops_final_verification();
    test_success_verifies_payload_and_committed_record();
    test_complete_sectors_are_verified_at_both_margins();
    return 0;
}
