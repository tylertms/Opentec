#include <assert.h>

#include "system/timing.h"

static void test_initial_values(void) {
    MotorServiceTiming timing;
    motor_service_timing_initialize(&timing);

    assert(timing.countdowns[kMotorCountdownTelemetry].ticks == 100U);
    assert(timing.countdowns[kMotorCountdownStartupInterlockB].ticks == 10U);
    assert(timing.countdowns[kMotorCountdownStartupRamp].ticks == 2000U);
    assert(timing.countdowns[kMotorCountdownEncoderIndex].ticks == 5000U);
    assert(timing.countdowns[kMotorCountdownStartupInterlockA].ticks == 200U);
    for (uint32_t index = 0U; index < MOTOR_SERVICE_COUNTDOWN_COUNT; ++index) {
        assert(timing.countdowns[index].active == 0U);
    }
}

int motor_test_service_timing(void) {
    test_initial_values();

    MotorServiceTiming timing = {
        .countdowns =
            {
                {.ticks = 2U, .active = 1U},
                {.ticks = 2U, .active = 0U},
                {.ticks = 2U, .active = 2U},
                {.ticks = 0U, .active = 1U},
                {.ticks = 1U, .active = 1U},
            },
    };

    for (uint32_t tick = 0U; tick < 9U; ++tick) {
        assert(!motor_service_timing_tick(&timing));
    }

    assert(timing.countdowns[0].ticks == 0U);
    assert(timing.countdowns[1].ticks == 2U);
    assert(timing.countdowns[2].ticks == 2U);
    assert(timing.countdowns[3].ticks == 0U);
    assert(timing.countdowns[4].ticks == 0U);
    assert(timing.derating_controller_ticks == 9U);
    assert(motor_service_timing_tick(&timing));
    assert(timing.derating_controller_ticks == 0U);
    assert(!motor_service_timing_tick(&timing));

    return 0;
}
