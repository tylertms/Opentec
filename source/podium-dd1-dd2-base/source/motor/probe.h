#ifndef OPENTEC_BASE_MOTOR_PROBE_H
#define OPENTEC_BASE_MOTOR_PROBE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"

typedef enum {
    MOTOR_PROBE_IDLE,
    MOTOR_PROBE_STATUS,
    MOTOR_PROBE_VERSION,
    MOTOR_PROBE_COMPLETE,
    MOTOR_PROBE_FAILED,
} MotorProbePhase;

typedef struct {
    MotorIdentity identity;
    MotorProbePhase phase;
    uint8_t status;
    uint8_t version[4];
    uint8_t failures;
    bool transfer_active;
} MotorProbe;

void motor_probe_init(MotorProbe *probe);
void motor_probe_start(MotorProbe *probe);
void motor_probe_run(MotorProbe *probe);
const MotorIdentity *motor_probe_identity(const MotorProbe *probe);

#endif
