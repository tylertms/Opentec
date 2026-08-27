#ifndef OPENTEC_BASE_MOTOR_TELEMETRY_SERVICE_H
#define OPENTEC_BASE_MOTOR_TELEMETRY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/telemetry.h"

typedef enum {
    MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE,
    MOTOR_TELEMETRY_READ_DRIVER_TEMPERATURE,
    MOTOR_TELEMETRY_READ_RUNTIME,
    MOTOR_TELEMETRY_READ_ACCESSORY_TYPE,
} MotorTelemetryRead;

typedef struct {
    MotorTelemetry telemetry;
    MotorTelemetryRead read;
    uint8_t data[4];
    uint32_t next_poll_ms;
    bool extended;
    bool transfer_active;
} MotorTelemetryService;

void motor_telemetry_service_init(MotorTelemetryService *service, const MotorIdentity *identity);
void motor_telemetry_service_run(MotorTelemetryService *service, uint32_t now_ms);
const MotorTelemetry *motor_telemetry_service_value(const MotorTelemetryService *service);

#endif
