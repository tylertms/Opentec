#ifndef OPENTEC_BASE_MOTOR_STATUS_SERVICE_H
#define OPENTEC_BASE_MOTOR_STATUS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/output_interlock.h"

typedef enum {
    MOTOR_STATUS_DISABLED,
    MOTOR_STATUS_INITIALIZE,
    MOTOR_STATUS_READ,
} MotorStatusPhase;

typedef struct {
    MotorOutputInterlock interlock;
    const MotorIdentity *identity;
    MotorStatusPhase phase;
    uint32_t next_read_ms;
    uint8_t status;
    bool transfer_active;
} MotorStatusService;

void motor_status_service_init(MotorStatusService *service, const MotorIdentity *identity);
void motor_status_service_run(MotorStatusService *service, uint32_t now_ms);
bool motor_status_service_output_inhibited(const MotorStatusService *service);

#endif
