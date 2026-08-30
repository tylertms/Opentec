#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/tuning_sync.h"

static MotorTuningContext default_context(void) {
    MotorTuningContext context = {
        .automatic_rotation_degrees = 1080,
        .ramp_percent = 100,
        .strength_percent = 100,
        .xbox_mode = 0,
        .calibration_active = 0,
        .extended_parameters = 1,
    };
    return context;
}

static void acknowledge_all(MotorTuningSync *sync) {
    MotorParameterWrite write;
    uint8_t count = 0;
    while (motor_tuning_sync_next(sync, &write)) {
        motor_tuning_sync_complete(sync, true);
        count++;
    }
    assert(count == MOTOR_TUNING_PARAMETER_COUNT);
}

static void test_initial_sync(void) {
    TuningProfile profile;
    MotorTuningContext context = default_context();
    MotorTuningSync sync;
    MotorParameterWrite write;
    tuning_profile_defaults(&profile);
    motor_tuning_sync_init(&sync, &profile, &context);

    assert(motor_tuning_sync_pending(&sync));
    for (uint8_t parameter = 0; parameter < MOTOR_TUNING_PARAMETER_COUNT; parameter++) {
        assert(motor_tuning_sync_next(&sync, &write));
        assert(write.address == 0x20 + parameter);
        assert(!motor_tuning_sync_next(&sync, &write));
        motor_tuning_sync_complete(&sync, true);
    }
    assert(!motor_tuning_sync_pending(&sync));
}

static void test_refresh_sends_only_changes(void) {
    TuningProfile profile;
    MotorTuningContext context = default_context();
    MotorTuningSync sync;
    MotorParameterWrite write;
    tuning_profile_defaults(&profile);
    motor_tuning_sync_init(&sync, &profile, &context);
    acknowledge_all(&sync);

    motor_tuning_sync_refresh(&sync, &profile, &context);
    assert(!motor_tuning_sync_pending(&sync));

    profile.natural_damper = 75;
    motor_tuning_sync_refresh(&sync, &profile, &context);
    assert(motor_tuning_sync_next(&sync, &write));
    assert(write.address == 0x23);
    assert(write.data[0] == 191);
    motor_tuning_sync_complete(&sync, true);
    assert(!motor_tuning_sync_pending(&sync));
}

static void test_legacy_sync_skips_extended_parameters(void) {
    static const uint8_t expected_addresses[] = {0x20, 0x21, 0x23, 0x24, 0x27, 0x28, 0x29, 0x2a};
    TuningProfile profile;
    MotorTuningContext context = default_context();
    MotorTuningSync sync;
    MotorParameterWrite write;
    tuning_profile_defaults(&profile);
    context.extended_parameters = 0;
    motor_tuning_sync_init(&sync, &profile, &context);

    for (uint8_t index = 0; index < sizeof(expected_addresses); ++index) {
        assert(motor_tuning_sync_next(&sync, &write));
        assert(write.address == expected_addresses[index]);
        motor_tuning_sync_complete(&sync, true);
    }
    assert(!motor_tuning_sync_pending(&sync));
}

static void test_failed_write_retries(void) {
    TuningProfile profile;
    MotorTuningContext context = default_context();
    MotorTuningSync sync;
    MotorParameterWrite first;
    MotorParameterWrite retry;
    tuning_profile_defaults(&profile);
    motor_tuning_sync_init(&sync, &profile, &context);

    assert(motor_tuning_sync_next(&sync, &first));
    motor_tuning_sync_complete(&sync, false);
    assert(motor_tuning_sync_next(&sync, &retry));
    assert(retry.address == first.address);
    assert(retry.length == first.length);
    assert(retry.data[0] == first.data[0]);
    assert(retry.data[1] == first.data[1]);
}

static void test_change_during_write_remains_pending(void) {
    TuningProfile profile;
    MotorTuningContext context = default_context();
    MotorTuningSync sync;
    MotorParameterWrite write;
    tuning_profile_defaults(&profile);
    motor_tuning_sync_init(&sync, &profile, &context);
    acknowledge_all(&sync);

    profile.force_feedback_strength = 60;
    motor_tuning_sync_refresh(&sync, &profile, &context);
    assert(motor_tuning_sync_next(&sync, &write));
    assert(write.address == 0x21);
    assert(write.data[0] == 60);

    profile.force_feedback_strength = 80;
    motor_tuning_sync_refresh(&sync, &profile, &context);
    motor_tuning_sync_complete(&sync, true);
    assert(motor_tuning_sync_next(&sync, &write));
    assert(write.address == 0x21);
    assert(write.data[0] == 80);
}

int main(void) {
    test_initial_sync();
    test_refresh_sends_only_changes();
    test_legacy_sync_skips_extended_parameters();
    test_failed_write_retries();
    test_change_during_write_remains_pending();
    return 0;
}
