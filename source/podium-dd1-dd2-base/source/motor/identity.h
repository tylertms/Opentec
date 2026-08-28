#ifndef OPENTEC_BASE_MOTOR_IDENTITY_H
#define OPENTEC_BASE_MOTOR_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_PROTOCOL_LEGACY,
    MOTOR_PROTOCOL_STANDARD,
    MOTOR_PROTOCOL_POSITION_A,
    MOTOR_PROTOCOL_POSITION_B,
} MotorProtocol;

typedef struct {
    MotorProtocol protocol;
    uint8_t version[4];
    uint8_t model;
    uint8_t transfer_code;
    uint8_t initial_status;
} MotorIdentity;

bool motor_identity_decode(uint8_t status, const uint8_t version[4], MotorIdentity *identity);
bool motor_identity_has_extended_parameters(const MotorIdentity *identity);
uint8_t motor_identity_input_transfer_code(const MotorIdentity *identity);

#endif
