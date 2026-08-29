#ifndef OPENTEC_BASE_I2C_PROBE_TRANSFER_SERVICE_H
#define OPENTEC_BASE_I2C_PROBE_TRANSFER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"
#include "i2c/probe_exchange_service.h"

typedef enum {
    I2C_PROBE_TRANSFER_SERVICE_IDLE,
    I2C_PROBE_TRANSFER_SERVICE_RUNNING,
    I2C_PROBE_TRANSFER_SERVICE_COMPLETE,
    I2C_PROBE_TRANSFER_SERVICE_FAILED,
} I2cProbeTransferServiceStatus;

typedef struct {
    I2cProbeTransferSequence sequence;
    I2cProbeExchangeService exchange;
    I2cProbeTransferStep current_step;
    I2cProbeTransferInput current_input;
    uint8_t request[I2C_PROBE_TRANSFER_WRITE_SIZE];
    uint8_t response[I2C_PROBE_TRANSFER_READ_SIZE];
    uint8_t current_payload_length;
    I2cProbeTransferServiceStatus status;
    I2cProbeExchangeResult result;
} I2cProbeTransferService;

void i2c_probe_transfer_service_init(I2cProbeTransferService *service);
bool i2c_probe_transfer_service_start(I2cProbeTransferService *service, const uint8_t *request,
                                      uint16_t request_length, bool checked);
void i2c_probe_transfer_service_run(I2cProbeTransferService *service);
I2cProbeTransferServiceStatus
i2c_probe_transfer_service_status(const I2cProbeTransferService *service);
I2cProbeExchangeResult i2c_probe_transfer_service_result(const I2cProbeTransferService *service);
const uint8_t *i2c_probe_transfer_service_response(const I2cProbeTransferService *service,
                                                   uint16_t *length);

#endif
