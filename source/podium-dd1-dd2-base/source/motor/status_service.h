#ifndef OPENTEC_BASE_MOTOR_STATUS_SERVICE_H
#define OPENTEC_BASE_MOTOR_STATUS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/output_interlock.h"

typedef enum {
    MOTOR_STATUS_DISABLED,
    MOTOR_STATUS_READ_COMMAND,
    MOTOR_STATUS_WRITE_COMMAND,
    MOTOR_STATUS_INITIALIZE,
    MOTOR_STATUS_READ,
} MotorStatusPhase;

typedef enum {
    MOTOR_STATUS_EVENT_NONE = 0,
    MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_SUCCEEDED = 3,
    MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_STARTED = 4,
    MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_FAILED = 5,
} MotorStatusEvent;

typedef struct {
    MotorOutputInterlock interlock;
    const MotorIdentity *identity;
    MotorStatusPhase phase;
    uint32_t next_cycle_ms;
    uint8_t command[2];
    uint8_t status;
    MotorStatusEvent event;
    bool command_pending;
    bool command_sent;
    bool status_initialized;
    bool transfer_active;
} MotorStatusService;

void motor_status_service_init(MotorStatusService *service, const MotorIdentity *identity);
void motor_status_service_request_command(MotorStatusService *service);
void motor_status_service_run(MotorStatusService *service, uint32_t now_ms);
MotorStatusEvent motor_status_service_take_event(MotorStatusService *service);
bool motor_status_service_output_inhibited(const MotorStatusService *service);

#endif
