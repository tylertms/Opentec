#include "motor/output_interlock.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"

enum {
    MOTOR_EXTENDED_COMMAND_INHIBIT_RESPONSE = 0xbbbb,
    MOTOR_EXTENDED_STATUS_INHIBIT_RESPONSE = 0xaa,
    MOTOR_STANDARD_STATUS_INHIBIT_RESPONSE = 0xff,
};

/**
 * @brief Clears the latched motor-output interlock.
 * @param[out] interlock Motor-output interlock state.
 */
void motor_output_interlock_init(MotorOutputInterlock *interlock) { interlock->engaged = false; }

/**
 * @brief Latches the motor-output interlock for the extended command fault response.
 * @param[in,out] interlock Motor-output interlock state.
 * @param[in] identity Identified motor-controller protocol.
 * @param[in] response Completed motor-command response word.
 */
void motor_output_interlock_accept_command(MotorOutputInterlock *interlock,
                                           const MotorIdentity *identity, uint16_t response) {
    if (motor_identity_has_extended_parameters(identity) &&
        response == MOTOR_EXTENDED_COMMAND_INHIBIT_RESPONSE) {
        interlock->engaged = true;
    }
}

/**
 * @brief Latches the motor-output interlock for the protocol-specific status fault response.
 * @param[in,out] interlock Motor-output interlock state.
 * @param[in] identity Identified motor-controller protocol.
 * @param[in] response Completed motor-status response byte.
 */
void motor_output_interlock_accept_status(MotorOutputInterlock *interlock,
                                          const MotorIdentity *identity, uint8_t response) {
    uint8_t inhibit_response = motor_identity_has_extended_parameters(identity)
                                   ? MOTOR_EXTENDED_STATUS_INHIBIT_RESPONSE
                                   : MOTOR_STANDARD_STATUS_INHIBIT_RESPONSE;
    if (response == inhibit_response) {
        interlock->engaged = true;
    }
}

/**
 * @brief Reports whether motor output has been irreversibly inhibited for this boot.
 * @param[in] interlock Motor-output interlock state.
 * @return True after a qualifying command or status response is accepted.
 */
bool motor_output_interlock_engaged(const MotorOutputInterlock *interlock) {
    return interlock->engaged;
}
