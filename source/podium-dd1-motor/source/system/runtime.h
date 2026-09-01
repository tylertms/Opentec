#ifndef OPENTEC_MOTOR_RUNTIME_H
#define OPENTEC_MOTOR_RUNTIME_H

/**
 * @brief Initializes the complete motor runtime and its interrupt graph.
 *
 * Selects the board profile, loads persistent calibration, configures peripherals, and starts the
 * motor in its safe startup interlock state.
 */
void motor_runtime_initialize(void);

/**
 * @brief Services deferred motor runtime work.
 *
 * Processes maintenance requests, force feedback, interpolation, thermal telemetry, and the
 * watchdog refresh from the firmware main loop.
 */
void motor_runtime_poll(void);

#endif
