#include "wheel/remote_spi.h"

#include <string.h>

enum {
    SPI_PAYLOAD_SIZE = WQR_FRAME_PAYLOAD_SIZE,
    SPI_CONTROL_OFFSET = WQR_FRAME_PAYLOAD_SIZE - 1,
    SPI_TRANSFER_ENABLED = 1,
    SPI_PEER_DETECTED = 2
};

static uint8_t payload_type(wheel_remote_spi_port port) {
    return port == WHEEL_REMOTE_SPI_PRIMARY ? WQR_PAYLOAD_PRIMARY_SPI : WQR_PAYLOAD_ALTERNATE_SPI;
}

bool wheel_remote_spi_begin(wheel_remote_exchange *exchange, uint8_t sequence,
                            wheel_remote_spi_port port, const uint8_t *transmit,
                            size_t transmit_length, bool enable_transfer) {
    uint8_t *request;
    size_t capacity = port == WHEEL_REMOTE_SPI_PRIMARY ? WHEEL_REMOTE_SPI_TRANSFER_SIZE : 2;

    if ((port != WHEEL_REMOTE_SPI_PRIMARY && port != WHEEL_REMOTE_SPI_ALTERNATE) ||
        transmit_length > capacity || (transmit == NULL && transmit_length != 0)) {
        return false;
    }

    request =
        wheel_remote_exchange_prepare(exchange, payload_type(port), sequence, SPI_PAYLOAD_SIZE);
    if (request == NULL) {
        return false;
    }

    memset(request, 0, SPI_PAYLOAD_SIZE);
    if (transmit_length != 0) {
        memcpy(request, transmit, transmit_length);
    }
    request[SPI_CONTROL_OFFSET] = enable_transfer ? SPI_TRANSFER_ENABLED : 0;
    return true;
}

bool wheel_remote_spi_finish(const wheel_remote_exchange *exchange,
                             wheel_remote_spi_result *result) {
    const uint8_t *response = wheel_remote_exchange_response(exchange);

    if (result == NULL || response == NULL ||
        wheel_remote_exchange_response_length(exchange) != SPI_PAYLOAD_SIZE ||
        (exchange->payload_type != WQR_PAYLOAD_PRIMARY_SPI &&
         exchange->payload_type != WQR_PAYLOAD_ALTERNATE_SPI)) {
        return false;
    }

    result->data = response;
    result->data_length =
        exchange->payload_type == WQR_PAYLOAD_PRIMARY_SPI ? WHEEL_REMOTE_SPI_TRANSFER_SIZE : 2;
    result->peer_detected = (response[SPI_CONTROL_OFFSET] & SPI_PEER_DETECTED) != 0;
    return true;
}
