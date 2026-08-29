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

void i2c_probe_handshake_init(I2cProbeHandshake *handshake);
I2cProbeResponseResult i2c_probe_handshake_evaluate(I2cProbeHandshake *handshake, uint8_t response);
I2cProbeResponseResult i2c_probe_command_response_evaluate(uint8_t response);
uint8_t i2c_probe_checksum(const uint8_t *payload, uint8_t payload_length);
I2cProbeValidationResult i2c_probe_final_response_validate(const I2cProbeFinalResponse *response,
                                                           bool checksum_enabled);

#endif
