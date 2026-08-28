#include "motor/identity.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Classifies a motor controller and extracts its model, version, and transfer code.
 *
 * Retains the complete version response, separates the six-bit transfer code, and selects the
 * legacy, standard, or extended position protocol from the initial status byte.
 *
 * @param[in] status Initial controller status byte.
 * @param[in] version Four-byte controller version response.
 * @param[out] identity Decoded controller identity, including partial fields on failure.
 * @return True for legacy, standard, and the two extended protocol encodings.
 */
bool motor_identity_decode(uint8_t status, const uint8_t version[4], MotorIdentity *identity) {
    identity->initial_status = status;
    for (uint8_t index = 0; index < sizeof(identity->version); index++) {
        identity->version[index] = version[index];
    }
    identity->transfer_code = version[0] & 0x3f;
    identity->model = 0;

    if ((status & 0x80) == 0) {
        identity->protocol = MOTOR_PROTOCOL_LEGACY;
        return true;
    }

    identity->model = (status >> 2) & 0x1f;
    switch (status & 3) {
    case 0:
        identity->protocol = MOTOR_PROTOCOL_STANDARD;
        return true;
    case 1:
        identity->protocol = MOTOR_PROTOCOL_POSITION_A;
        return true;
    case 2:
        identity->protocol = MOTOR_PROTOCOL_POSITION_B;
        return true;
    default:
        return false;
    }
}

/**
 * @brief Reports whether a motor protocol supports the extended parameter exchange.
 *
 * Accepts either position-capable protocol and rejects the legacy and standard protocols.
 *
 * @param[in] identity Decoded motor-controller identity.
 * @return True for either extended position protocol.
 */
bool motor_identity_has_extended_parameters(const MotorIdentity *identity) {
    return identity->protocol == MOTOR_PROTOCOL_POSITION_A ||
           identity->protocol == MOTOR_PROTOCOL_POSITION_B;
}

/**
 * @brief Selects the motor-controller code published in the input report.
 *
 * Returns the low six-bit identification code for standard and position-capable controllers.
 * Legacy and unavailable controllers publish zero.
 *
 * @param[in] identity Decoded motor-controller identity, or null when unavailable.
 * @return Input-report transfer code.
 */
uint8_t motor_identity_input_transfer_code(const MotorIdentity *identity) {
    return identity != 0 && identity->protocol != MOTOR_PROTOCOL_LEGACY ? identity->transfer_code
                                                                        : 0;
}
