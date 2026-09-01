#include "platform/storage.h"

#include <assert.h>
#include <string.h>

static MotorEncoderCalibrationRecord record;

static void test_invalid_record_is_not_programmed(void) {
    memset(&record, 0, sizeof(record));
    assert(motor_calibration_storage_program(&record) == kStatus_Fail);
}

static void test_real_flash_round_trip(void) {
    memset(&record, 0, sizeof(record));
    record.magic = UINT32_C(0xaaaaaaaa);
    record.version = 4U;
    record.correction_scale = 0x3333U;
    record.sample_offset = 2U;
    for (size_t index = 0U; index < MOTOR_ENCODER_CORRECTION_CAPACITY; ++index) {
        record.forward[index] = (int16_t)index;
        record.reverse[index] = (int16_t)-(int32_t)index;
    }
    motor_encoder_calibration_record_finalize(&record);

    assert(motor_calibration_storage_erase() == kStatus_Success);
    assert(!motor_calibration_storage_load(&record));
    for (size_t index = 0U; index < sizeof(record); ++index)
        assert(((const uint8_t *)&record)[index] == 0U);

    record.magic = UINT32_C(0xaaaaaaaa);
    record.version = 4U;
    record.correction_scale = 0x3333U;
    record.sample_offset = 2U;
    for (size_t index = 0U; index < MOTOR_ENCODER_CORRECTION_CAPACITY; ++index) {
        record.forward[index] = (int16_t)index;
        record.reverse[index] = (int16_t)-(int32_t)index;
    }
    motor_encoder_calibration_record_finalize(&record);
    assert(motor_calibration_storage_program(&record) == kStatus_Success);
    assert(*(const uint32_t *)(uintptr_t)0x0001d800U == UINT32_C(0xaaaaaaaa));
    assert(*(const uint32_t *)(uintptr_t)0x0001d804U == 4U);
    memset(&record, 0, sizeof(record));
    assert(motor_calibration_storage_load(&record));
    assert(record.magic == UINT32_C(0xaaaaaaaa));
    assert(record.version == 4U);
    assert(record.correction_scale == 0x3333U);
    assert(record.sample_offset == 2U);
    for (size_t index = 0U; index < MOTOR_ENCODER_CORRECTION_CAPACITY; ++index) {
        assert(record.forward[index] == (int16_t)index);
        assert(record.reverse[index] == (int16_t)-(int32_t)index);
    }
    assert(motor_calibration_storage_erase() == kStatus_Success);
    assert(!motor_calibration_storage_load(&record));
}

int motor_test_storage(void) {
    assert(motor_calibration_storage_initialize() == kStatus_Success);
    test_invalid_record_is_not_programmed();
    test_real_flash_round_trip();
    return 0;
}
