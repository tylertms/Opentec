#include "motor/startup_centering.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "platform/time.h"

/**
 * @brief Auxiliary-bus settings and timing for motor startup centering.
 */
enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78, /**< Auxiliary-bus address of the motor controller. */
    MOTOR_STARTUP_READINESS_REGISTER =
        0x08, /**< Register containing the startup-readiness value. */
    MOTOR_STARTUP_READINESS_TIMEOUT_MS = 5000, /**< Maximum readiness wait in milliseconds. */
    MOTOR_STARTUP_CENTERING_DURATION_MS =
        4000,                              /**< Duration of active centering in milliseconds. */
    MOTOR_STARTUP_CENTERING_STEP_MS = 400, /**< Time step used by the centering force envelope. */
    MOTOR_STARTUP_CENTERING_GAIN = 12,     /**< Position-to-force gain during startup centering. */
};

/**
 * @brief Starts the motor-controller readiness and wheel-centering sequence.
 *
 * Opens a five-second motor-readiness window immediately, allowing failed readiness transactions to
 * time out without preventing normal startup.
 *
 * @param[out] centering Startup-centering state to initialize.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_startup_centering_init(MotorStartupCentering *centering, uint32_t now_ms) {
    *centering = (MotorStartupCentering){
        .phase = MOTOR_STARTUP_CENTERING_WAITING,
        .deadline_ms = now_ms + MOTOR_STARTUP_READINESS_TIMEOUT_MS,
    };
}

/**
 * @brief Finishes an active startup-parameter transfer.
 *
 * Releases the auxiliary bus. A successful nonzero readiness read starts the four-second
 * centering interval. Failed operations and zero-valued readiness reads remain eligible for retry
 * until the fixed startup deadline.
 *
 * @param[in,out] centering Startup-centering state and parameter storage.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void finish_transfer(MotorStartupCentering *centering, uint32_t now_ms) {
    PlatformAuxBusStatus status = platform_aux_bus_status();
    if (!centering->transfer_active || status == PLATFORM_AUX_BUS_BUSY) {
        return;
    }

    platform_aux_bus_clear();
    centering->transfer_active = false;
    if (centering->phase == MOTOR_STARTUP_CENTERING_WAITING &&
        status == PLATFORM_AUX_BUS_SUCCEEDED && centering->parameter_value != 0) {
        centering->phase = MOTOR_STARTUP_CENTERING_ACTIVE;
        centering->deadline_ms = now_ms + MOTOR_STARTUP_CENTERING_DURATION_MS;
    }
}

/**
 * @brief Advances the motor readiness wait.
 *
 * Ends the sequence at the five-second deadline or starts a one-byte read from motor parameter
 * eight while the shared bus is idle.
 *
 * @param[in,out] centering Startup-centering state and transfer ownership.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void wait_for_readiness(MotorStartupCentering *centering, uint32_t now_ms) {
    if (centering->phase != MOTOR_STARTUP_CENTERING_WAITING) {
        return;
    }
    if (platform_time_reached(now_ms, centering->deadline_ms)) {
        centering->phase = MOTOR_STARTUP_CENTERING_COMPLETE;
        return;
    }
    if (platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE) {
        centering->transfer_active =
            platform_aux_bus_start_read(MOTOR_AUX_BUS_ADDRESS, MOTOR_STARTUP_READINESS_REGISTER,
                                        &centering->parameter_value, 1);
    }
}

/**
 * @brief Calculates the restoring force for the active centering interval.
 *
 * Applies a gain of minus twelve to centered wheel position and constrains it with the elapsed-time
 * envelope `elapsed * (elapsed / 400 + 1)`, limited to the unsigned 16-bit force range.
 *
 * @param[in] centered_position Current wheel position relative to the retained center.
 * @param[in] elapsed_ms Elapsed centering time in milliseconds.
 * @return Signed restoring force within the current startup envelope.
 */
static int32_t calculate_centering_force(int32_t centered_position, uint32_t elapsed_ms) {
    uint32_t multiplier = elapsed_ms / MOTOR_STARTUP_CENTERING_STEP_MS + 1;
    uint32_t limit = elapsed_ms > UINT16_MAX / multiplier ? UINT16_MAX : elapsed_ms * multiplier;
    int32_t position_limit = (int32_t)(limit / MOTOR_STARTUP_CENTERING_GAIN);
    if (centered_position > position_limit) {
        return -(int32_t)limit;
    }
    if (centered_position < -position_limit) {
        return (int32_t)limit;
    }
    return centered_position * -MOTOR_STARTUP_CENTERING_GAIN;
}

/**
 * @brief Advances startup readiness and wheel centering.
 *
 * Retries the readiness parameter until it becomes nonzero or five seconds elapse. A ready motor
 * receives a restoring force for four seconds; missing position input produces zero force without
 * extending that interval.
 *
 * @param[in,out] centering Startup-centering state and transfer ownership.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] position_available True when centered_position contains a current motor sample.
 * @param[in] centered_position Current wheel position relative to the retained center.
 * @return Signed startup restoring force for the current iteration.
 */
int32_t motor_startup_centering_run(MotorStartupCentering *centering, uint32_t now_ms,
                                    bool position_available, int32_t centered_position) {
    finish_transfer(centering, now_ms);
    if (centering->phase == MOTOR_STARTUP_CENTERING_WAITING) {
        wait_for_readiness(centering, now_ms);
    }
    if (centering->phase != MOTOR_STARTUP_CENTERING_ACTIVE) {
        return 0;
    }

    uint32_t started_ms = centering->deadline_ms - MOTOR_STARTUP_CENTERING_DURATION_MS;
    uint32_t elapsed_ms = now_ms - started_ms;
    int32_t force =
        position_available ? calculate_centering_force(centered_position, elapsed_ms) : 0;
    if (platform_time_reached(now_ms, centering->deadline_ms)) {
        centering->phase = MOTOR_STARTUP_CENTERING_COMPLETE;
    }
    return force;
}

/**
 * @brief Reports whether startup centering currently owns motor force output.
 *
 * Distinguishes the four-second movement interval from readiness waiting and completed states.
 *
 * @param[in] centering Startup-centering state.
 * @return True during the active centering interval; otherwise false.
 */
bool motor_startup_centering_active(const MotorStartupCentering *centering) {
    return centering->phase == MOTOR_STARTUP_CENTERING_ACTIVE;
}

/**
 * @brief Reports whether the startup-centering sequence has ended.
 *
 * Includes both readiness timeout and completion of the four-second movement interval.
 *
 * @param[in] centering Startup-centering state.
 * @return True after the sequence ends; otherwise false.
 */
bool motor_startup_centering_complete(const MotorStartupCentering *centering) {
    return centering->phase == MOTOR_STARTUP_CENTERING_COMPLETE && !centering->transfer_active;
}
