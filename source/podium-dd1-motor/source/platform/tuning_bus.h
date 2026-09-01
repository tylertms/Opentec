#ifndef OPENTEC_MOTOR_COMMUNICATION_H
#define OPENTEC_MOTOR_COMMUNICATION_H

#include "tuning/parameter.h"

/**
 * @brief Handles an accepted live motor-parameter change.
 *
 * The callback runs after the parameter bank accepts a write to a live-control setting.
 *
 * @param[in] context Caller context supplied during parameter-bus initialization.
 */
typedef void (*MotorParameterChangedHandler)(void *context);

/**
 * @brief Initializes the motor parameter service on its I2C bus.
 *
 * The supplied parameter bank and change callback are installed before the nonblocking slave
 * transfer engine is started.
 *
 * @param[in] parameters Parameter bank shared with the motor runtime.
 * @param[in] changed_handler Function invoked after an accepted live-control change.
 * @param[in] context Caller context passed to the change handler.
 */
void motor_bus_initialize(MotorParameterBank *parameters,
                          MotorParameterChangedHandler changed_handler, void *context);

/**
 * @brief Recovers a parameter-bus transaction that remains active for ten service ticks.
 *
 * A stalled transfer is aborted and the slave configuration is restored.
 */
void motor_bus_service(void);

#endif
