#include "common/motor/service.h"

/**
 * @brief Advances five gated countdowns and the ten-tick velocity-control cadence.
 * @param timing Periodic motor-service timing state.
 * @return True every tenth service tick.
 */
bool motor_service_timing_tick(MotorServiceTiming *timing) {
    for (uint32_t index = 0U; index < MOTOR_SERVICE_COUNTDOWN_COUNT; ++index) {
        MotorCountdown *countdown = &timing->countdowns[index];
        if (countdown->active == 1U && countdown->ticks != 0U) {
            --countdown->ticks;
        }
    }

    ++timing->velocity_controller_ticks;
    if (timing->velocity_controller_ticks > 9U) {
        timing->velocity_controller_ticks = 0U;
        return true;
    }

    return false;
}
