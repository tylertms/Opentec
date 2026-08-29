#ifndef OPENTEC_BASE_I2C_PROBE_STARTUP_SERVICE_H
#define OPENTEC_BASE_I2C_PROBE_STARTUP_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"

typedef enum {
    I2C_PROBE_STARTUP_SERVICE_IDLE,
    I2C_PROBE_STARTUP_SERVICE_RUNNING,
    I2C_PROBE_STARTUP_SERVICE_COMPLETE,
} I2cProbeStartupServiceStatus;

enum {
    I2C_PROBE_STARTUP_RESPONSE_CAPACITY = 0x1f,
};

typedef struct {
    I2cProbeStartup startup;
    I2cProbeCommand active_command;
    uint8_t response[I2C_PROBE_STARTUP_RESPONSE_CAPACITY];
    I2cProbeStartupResponse response_view;
    I2cProbeStartupServiceStatus status;
    bool transfer_active;
} I2cProbeStartupService;

void i2c_probe_startup_service_init(I2cProbeStartupService *service);
void i2c_probe_startup_service_start(I2cProbeStartupService *service);
void i2c_probe_startup_service_run(I2cProbeStartupService *service, uint32_t now_ms);
I2cProbeStartupServiceStatus
i2c_probe_startup_service_status(const I2cProbeStartupService *service);

#endif
