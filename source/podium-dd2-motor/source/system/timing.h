#ifndef OPENTEC_MOTOR_SERVICE_H
#define OPENTEC_MOTOR_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of countdown slots in the motor service timing state. */
#define MOTOR_SERVICE_COUNTDOWN_COUNT 5U

/**
 * @brief Identifies a countdown maintained by the motor service timer.
 */
typedef enum {
    kMotorCountdownTelemetry, /**< Reserved telemetry service countdown slot. */
    kMotorCountdownStartupInterlockB, /**< Countdown for the second startup interlock. */
    kMotorCountdownStartupRamp, /**< Countdown for rotor-alignment current ramping. */
    kMotorCountdownEncoderIndex, /**< Countdown for encoder-index search. */
    kMotorCountdownStartupInterlockA, /**< Countdown for the first startup interlock. */
} MotorCountdownIndex;

/**
 * @brief Stores one service countdown and its activation state.
 */
typedef struct {
    uint16_t ticks; /**< Remaining service ticks before the countdown expires. */
    uint16_t active; /**< Equal to one when the countdown is being decremented. */
} MotorCountdown;

/**
 * @brief Stores countdowns and cadence state for the motor service timer.
 */
typedef struct {
    MotorCountdown countdowns[MOTOR_SERVICE_COUNTDOWN_COUNT]; /**< Per-operation countdowns. */
    uint16_t derating_controller_ticks; /**< Service ticks since the last derating update. */
} MotorServiceTiming;

/**
 * @brief Initializes all motor service countdowns and cadence state.
 *
 * Startup, encoder-index, telemetry, and interlock countdowns receive their default values, and
 * the derating cadence resets.
 *
 * @param[out] timing Timing state to initialize.
 */
void motor_service_timing_initialize(MotorServiceTiming *timing);

/**
 * @brief Advances active motor service countdowns and the derating cadence.
 *
 * Each active countdown decrements toward zero, and the derating event is emitted once per ten
 * service ticks.
 *
 * @param[in,out] timing Timing state to advance.
 * @return True when the derating controller should run on this tick; otherwise false.
 */
bool motor_service_timing_tick(MotorServiceTiming *timing);

#endif
