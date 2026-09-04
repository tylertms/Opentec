#include "motor/identity.h"

#include <stdbool.h>
#include <stdint.h>

bool motor_identity_decode(uint8_t status, const uint8_t version[4], MotorIdentity *identity) {
    identity->initial_status = status;
    for (uint8_t index = 0; index < sizeof(identity->version); index++) {
        identity->version[index] = version[index];
    }
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
    case 2:
        identity->protocol = MOTOR_PROTOCOL_POSITION;
        return true;
    default:
        return false;
    }
}

bool motor_identity_has_extended_parameters(const MotorIdentity *identity) {
    return identity->protocol == MOTOR_PROTOCOL_POSITION;
}

uint8_t motor_identity_runtime_state(const MotorIdentity *identity) {
    return identity == 0 ? 0 : (uint8_t)identity->protocol + 1;
}
