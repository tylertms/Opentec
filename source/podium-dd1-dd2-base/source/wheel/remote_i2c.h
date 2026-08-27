#ifndef OPENTEC_BASE_WHEEL_REMOTE_I2C_H
#define OPENTEC_BASE_WHEEL_REMOTE_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wheel/remote_exchange.h"

enum { WHEEL_REMOTE_I2C_DATA_CAPACITY = WHEEL_REMOTE_EXCHANGE_CAPACITY - 2 };

typedef struct {
    const uint8_t *data;
    size_t data_length;
    uint8_t address;
    bool succeeded;
} wheel_remote_i2c_result;

bool wheel_remote_i2c_write_begin(wheel_remote_exchange *exchange, uint8_t sequence,
                                  uint8_t address, const uint8_t *data, size_t data_length);
bool wheel_remote_i2c_read_begin(wheel_remote_exchange *exchange, uint8_t sequence, uint8_t address,
                                 uint8_t command, size_t data_length);
bool wheel_remote_i2c_finish(const wheel_remote_exchange *exchange,
                             wheel_remote_i2c_result *result);

#endif
