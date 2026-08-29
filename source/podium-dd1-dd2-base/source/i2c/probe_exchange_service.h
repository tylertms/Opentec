#ifndef OPENTEC_BASE_I2C_PROBE_EXCHANGE_SERVICE_H
#define OPENTEC_BASE_I2C_PROBE_EXCHANGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"

typedef enum {
    I2C_PROBE_EXCHANGE_SERVICE_IDLE,
    I2C_PROBE_EXCHANGE_SERVICE_RUNNING,
    I2C_PROBE_EXCHANGE_SERVICE_COMPLETE,
    I2C_PROBE_EXCHANGE_SERVICE_FAILED,
} I2cProbeExchangeServiceStatus;

enum {
    I2C_PROBE_EXCHANGE_RESPONSE_CAPACITY = I2C_PROBE_TRANSFER_CHUNK_CAPACITY + 4,
};

typedef struct {
    I2cProbeExchange exchange;
    I2cProbeTransferFrame frame;
    uint8_t status_response[2];
    uint8_t response[I2C_PROBE_EXCHANGE_RESPONSE_CAPACITY];
    I2cProbeTransferResponse parsed_response;
    I2cProbeExchangeServiceStatus status;
    bool transfer_active;
} I2cProbeExchangeService;

void i2c_probe_exchange_service_init(I2cProbeExchangeService *service);
bool i2c_probe_exchange_service_start(I2cProbeExchangeService *service,
                                      const I2cProbeTransferFrame *frame);
void i2c_probe_exchange_service_run(I2cProbeExchangeService *service);
const uint8_t *i2c_probe_exchange_service_payload(const I2cProbeExchangeService *service,
                                                  uint8_t *length);
I2cProbeExchangeServiceStatus
i2c_probe_exchange_service_status(const I2cProbeExchangeService *service);
I2cProbeExchangeResult i2c_probe_exchange_service_result(const I2cProbeExchangeService *service);

#endif
