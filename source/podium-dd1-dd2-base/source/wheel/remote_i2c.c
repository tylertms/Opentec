#include "wheel/remote_i2c.h"

#include <string.h>

enum {
    I2C_RESERVED_OFFSET = 0,
    I2C_ADDRESS_OFFSET = 1,
    I2C_DATA_OFFSET = 2,
    I2C_READ_COMMAND_OFFSET = 2,
    I2C_READ_LENGTH_OFFSET = 3,
    I2C_READ_REQUEST_SIZE = 5,
    I2C_RESPONSE_DATA_OFFSET = 2
};

static bool address_valid(uint8_t address) { return address <= 0x7f; }

bool wheel_remote_i2c_write_begin(wheel_remote_exchange *exchange, uint8_t sequence,
                                  uint8_t address, const uint8_t *data, size_t data_length) {
    uint8_t *request;

    if (!address_valid(address) || data == NULL || data_length == 0 ||
        data_length > WHEEL_REMOTE_I2C_DATA_CAPACITY) {
        return false;
    }

    request = wheel_remote_exchange_prepare(exchange, WQR_PAYLOAD_I2C, sequence,
                                            data_length + I2C_DATA_OFFSET);
    if (request == NULL) {
        return false;
    }
    request[I2C_RESERVED_OFFSET] = 0;
    request[I2C_ADDRESS_OFFSET] = (uint8_t)(address << 1);
    memcpy(request + I2C_DATA_OFFSET, data, data_length);
    return true;
}

bool wheel_remote_i2c_read_begin(wheel_remote_exchange *exchange, uint8_t sequence, uint8_t address,
                                 uint8_t command, size_t data_length) {
    uint8_t *request;

    if (!address_valid(address) || data_length > WHEEL_REMOTE_I2C_DATA_CAPACITY) {
        return false;
    }

    request =
        wheel_remote_exchange_prepare(exchange, WQR_PAYLOAD_I2C, sequence, I2C_READ_REQUEST_SIZE);
    if (request == NULL) {
        return false;
    }
    request[I2C_RESERVED_OFFSET] = 0;
    request[I2C_ADDRESS_OFFSET] = (uint8_t)((address << 1) | 1);
    request[I2C_READ_COMMAND_OFFSET] = command;
    request[I2C_READ_LENGTH_OFFSET] = (uint8_t)data_length;
    request[I2C_READ_LENGTH_OFFSET + 1] = (uint8_t)(data_length >> 8);
    return true;
}

bool wheel_remote_i2c_finish(const wheel_remote_exchange *exchange,
                             wheel_remote_i2c_result *result) {
    const uint8_t *response = wheel_remote_exchange_response(exchange);
    size_t response_length = wheel_remote_exchange_response_length(exchange);
    bool read;
    size_t expected_length;

    if (result == NULL || response == NULL || exchange->request_length < I2C_DATA_OFFSET ||
        response_length < I2C_RESPONSE_DATA_OFFSET ||
        response[I2C_ADDRESS_OFFSET] != exchange->request[I2C_ADDRESS_OFFSET]) {
        return false;
    }

    read = (exchange->request[I2C_ADDRESS_OFFSET] & 1) != 0;
    if (read && exchange->request_length < I2C_READ_REQUEST_SIZE) {
        return false;
    }
    expected_length = read ? (size_t)exchange->request[I2C_READ_LENGTH_OFFSET] |
                                 (size_t)exchange->request[I2C_READ_LENGTH_OFFSET + 1] << 8
                           : 1;
    result->succeeded = response[0] == 1;
    result->address = exchange->request[I2C_ADDRESS_OFFSET] >> 1;
    result->data = response + I2C_RESPONSE_DATA_OFFSET;
    result->data_length = result->succeeded ? expected_length : 0;
    return !result->succeeded || response_length == expected_length + I2C_RESPONSE_DATA_OFFSET;
}
