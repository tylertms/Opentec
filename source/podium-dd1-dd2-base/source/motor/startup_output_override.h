#ifndef OPENTEC_BASE_MOTOR_STARTUP_OUTPUT_OVERRIDE_H
#define OPENTEC_BASE_MOTOR_STARTUP_OUTPUT_OVERRIDE_H

#include <stdbool.h>

#include "motor/identity.h"

/**
 * @brief Writes the startup output override to a recognized motor controller.
 *
 * The reference startup path clears its completion byte at 0x0426c8, then services the shared
 * transaction at 0x0426e8–0x0426ee until that byte changes. A pre-existing busy transfer is
 * completed first. The accepted override write is serviced until a terminal bus state is
 * published; terminal failure is not interpreted or retried.
 *
 * @param[in] identity Identified motor controller.
 * @return True when an accepted register write reaches a terminal state; false when it cannot
 * start.
 */
bool motor_startup_output_override_write(const MotorIdentity *identity);

#endif
