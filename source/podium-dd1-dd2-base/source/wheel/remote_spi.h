#ifndef OPENTEC_BASE_WHEEL_REMOTE_SPI_H
#define OPENTEC_BASE_WHEEL_REMOTE_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wheel/remote_exchange.h"

enum { WHEEL_REMOTE_SPI_TRANSFER_SIZE = 33 };

typedef enum { WHEEL_REMOTE_SPI_PRIMARY, WHEEL_REMOTE_SPI_ALTERNATE } wheel_remote_spi_port;

typedef struct {
    const uint8_t *data;
    size_t data_length;
    bool peer_detected;
} wheel_remote_spi_result;

bool wheel_remote_spi_begin(wheel_remote_exchange *exchange, uint8_t sequence,
                            wheel_remote_spi_port port, const uint8_t *transmit,
                            size_t transmit_length, bool enable_transfer);
bool wheel_remote_spi_finish(const wheel_remote_exchange *exchange,
                             wheel_remote_spi_result *result);

#endif
