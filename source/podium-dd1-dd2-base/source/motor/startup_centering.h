#ifndef OPENTEC_BASE_MOTOR_STARTUP_CENTERING_H
#define OPENTEC_BASE_MOTOR_STARTUP_CENTERING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Phases of motor startup readiness and centering.
 *
 * Separates the readiness read, active restoring-force interval, and terminal startup state.
 */
typedef enum {
    MOTOR_STARTUP_CENTERING_IDLE,    /**< Startup centering has not been initialized. */
    MOTOR_STARTUP_CENTERING_WAITING, /**< Waiting for a nonzero motor-readiness value. */
    MOTOR_STARTUP_CENTERING_ACTIVE,  /**< Applying restoring force during the centering interval. */
    MOTOR_STARTUP_CENTERING_COMPLETE, /**< Readiness timed out or centering finished. */
} MotorStartupCenteringPhase;

/**
 * @brief Motor startup readiness and centering state.
 *
 * Stores the current phase, timing deadline, readiness response, and auxiliary-bus transfer state.
 */
typedef struct {
    MotorStartupCenteringPhase phase; /**< Current readiness or centering phase. */
    uint32_t deadline_ms;             /**< Monotonic deadline for readiness or active centering. */
    uint8_t parameter_value;          /**< Latest motor-readiness parameter response. */
    bool transfer_active; /**< True while the readiness parameter read is in progress. */
} MotorStartupCentering;

/**
 * @brief Starts motor startup readiness and centering.
 *
 * Enters the readiness phase and sets the five-second readiness deadline from the supplied time.
 *
 * @param[out] centering Startup readiness and centering state to initialize.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_startup_centering_init(MotorStartupCentering *centering, uint32_t now_ms);

/**
 * @brief Advances motor startup readiness and centering.
 *
 * Services the readiness read and, once ready, returns the bounded restoring force for the current
 * centered position until the centering interval ends.
 *
 * @param[in,out] centering Startup readiness and centering state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] position_available True when centered_position contains a current motor sample.
 * @param[in] centered_position Current wheel position relative to the retained center.
 * @return Signed startup restoring force for the current iteration, or zero outside centering.
 */
int32_t motor_startup_centering_run(MotorStartupCentering *centering, uint32_t now_ms,
                                    bool position_available, int32_t centered_position);

/**
 * @brief Reports whether startup centering is applying force.
 *
 * Returns true only during the active centering interval.
 *
 * @param[in] centering Startup readiness and centering state.
 * @return True while startup centering currently owns motor force output.
 */
bool motor_startup_centering_active(const MotorStartupCentering *centering);

/**
 * @brief Reports whether startup readiness and centering have ended.
 *
 * Includes readiness timeout and completion of the active centering interval after any transfer has
 * finished.
 *
 * @param[in] centering Startup readiness and centering state.
 * @return True after the startup sequence has ended.
 */
bool motor_startup_centering_complete(const MotorStartupCentering *centering);

#endif
