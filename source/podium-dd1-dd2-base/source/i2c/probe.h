#ifndef OPENTEC_BASE_I2C_PROBE_H
#define OPENTEC_BASE_I2C_PROBE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    I2C_PROBE_RESPONSE_RETRY,
    I2C_PROBE_RESPONSE_BUSY,
    I2C_PROBE_RESPONSE_ACCEPTED,
    I2C_PROBE_RESPONSE_REJECTED,
} I2cProbeResponseResult;

typedef enum {
    I2C_PROBE_VALID,
    I2C_PROBE_CHECKSUM_ERROR,
    I2C_PROBE_STATUS_ERROR,
} I2cProbeValidationResult;

typedef struct {
    uint8_t retry_count;
} I2cProbeHandshake;

typedef struct {
    const uint8_t *payload;
    uint8_t payload_length;
    uint8_t expected_checksum;
    uint8_t primary_status;
    uint8_t secondary_status;
} I2cProbeFinalResponse;

typedef enum {
    I2C_PROBE_BEGIN_SESSION = 1,
    I2C_PROBE_READ_STARTUP_STATUS = 2,
    I2C_PROBE_READ_SIGNATURE = 3,
    I2C_PROBE_READ_CONFIRMATION = 4,
    I2C_PROBE_READ_READY_STATUS = 5,
} I2cProbeCommand;

typedef struct {
    uint8_t selector;
    uint8_t response_length;
} I2cProbeRequest;

void i2c_probe_handshake_init(I2cProbeHandshake *handshake);
I2cProbeResponseResult i2c_probe_handshake_evaluate(I2cProbeHandshake *handshake, uint8_t response);
I2cProbeResponseResult i2c_probe_command_response_evaluate(uint8_t response);
uint8_t i2c_probe_checksum(const uint8_t *payload, uint8_t payload_length);
I2cProbeValidationResult i2c_probe_final_response_validate(const I2cProbeFinalResponse *response,
                                                           bool checksum_enabled);
bool i2c_probe_request_encode(I2cProbeCommand command, I2cProbeRequest *request);

#endif
