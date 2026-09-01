#ifndef OPENTEC_BASE_MOTOR_OUTPUT_INTERLOCK_H
#define OPENTEC_BASE_MOTOR_OUTPUT_INTERLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"

/**
 * @brief Latched motor-output inhibit state.
 *
 * Records whether a protocol response has permanently inhibited force output for the current
 * boot.
 */
typedef struct {
    bool engaged; /**< True after a qualifying motor command or status response. */
} MotorOutputInterlock;

/**
 * @brief Initializes a motor-output interlock.
 *
 * Clears the inhibit latch so motor force output is initially permitted.
 *
 * @param[out] interlock Motor-output interlock state to initialize.
 */
void motor_output_interlock_init(MotorOutputInterlock *interlock);

/**
 * @brief Accepts a completed motor-command response.
 *
 * Latches output inhibition when an extended-parameter controller returns its command fault word.
 *
 * @param[in,out] interlock Motor-output interlock state.
 * @param[in] identity Identified motor-controller protocol.
 * @param[in] response Completed motor-command response word.
 */
void motor_output_interlock_accept_command(MotorOutputInterlock *interlock,
                                           const MotorIdentity *identity, uint16_t response);

/**
 * @brief Accepts a completed motor-status response.
 *
 * Latches output inhibition for the protocol-specific fault byte and ignores legacy controllers.
 *
 * @param[in,out] interlock Motor-output interlock state.
 * @param[in] identity Identified motor-controller protocol.
 * @param[in] response Completed motor-status response byte.
 */
void motor_output_interlock_accept_status(MotorOutputInterlock *interlock,
                                          const MotorIdentity *identity, uint8_t response);

/**
 * @brief Reports whether motor output is inhibited.
 *
 * Reads the latch set by a qualifying command or status response.
 *
 * @param[in] interlock Motor-output interlock state.
 * @return True when motor output has been inhibited for this boot.
 */
bool motor_output_interlock_engaged(const MotorOutputInterlock *interlock);

#endif
