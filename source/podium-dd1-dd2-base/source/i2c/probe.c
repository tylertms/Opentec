#include "i2c/probe.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    I2C_PROBE_RESPONSE_ACCEPT = 0x07,
    I2C_PROBE_RESPONSE_WAIT = 0x17,
    I2C_PROBE_UNEXPECTED_RETRY_LIMIT = 2,
};

/**
 * @brief Initializes probe handshake response tracking.
 *
 * Clears the retry count used to limit repeated unexpected handshake responses.
 *
 * @param[out] handshake Handshake response state to initialize.
 */
void i2c_probe_handshake_init(I2cProbeHandshake *handshake) { handshake->retry_count = 0; }

/**
 * @brief Evaluates one probe handshake response.
 *
 * Response 0x07 accepts the handshake and clears the retry count. Response 0x17 reports that the
 * device is busy and increments the count without applying the unexpected-response limit. Other
 * responses receive another attempt while the prior count is at most two and are then rejected.
 *
 * @param[in,out] handshake Persistent handshake retry state.
 * @param[in] response Device response code.
 * @return Accepted, busy, retry, or rejected response disposition.
 */
I2cProbeResponseResult i2c_probe_handshake_evaluate(I2cProbeHandshake *handshake,
                                                    uint8_t response) {
    if (response == I2C_PROBE_RESPONSE_ACCEPT) {
        handshake->retry_count = 0;
        return I2C_PROBE_RESPONSE_ACCEPTED;
    }
    if (response == I2C_PROBE_RESPONSE_WAIT) {
        ++handshake->retry_count;
        return I2C_PROBE_RESPONSE_BUSY;
    }
    if (handshake->retry_count <= I2C_PROBE_UNEXPECTED_RETRY_LIMIT) {
        ++handshake->retry_count;
        return I2C_PROBE_RESPONSE_RETRY;
    }
    return I2C_PROBE_RESPONSE_REJECTED;
}

/**
 * @brief Evaluates one command-stage probe response.
 *
 * Response 0x07 accepts the command, response 0x17 keeps it pending, and every other response
 * rejects the exchange.
 *
 * @param[in] response Device response code.
 * @return Accepted, busy, or rejected response disposition.
 */
I2cProbeResponseResult i2c_probe_command_response_evaluate(uint8_t response) {
    if (response == I2C_PROBE_RESPONSE_ACCEPT) {
        return I2C_PROBE_RESPONSE_ACCEPTED;
    }
    if (response == I2C_PROBE_RESPONSE_WAIT) {
        return I2C_PROBE_RESPONSE_BUSY;
    }
    return I2C_PROBE_RESPONSE_REJECTED;
}

/**
 * @brief Calculates the probe response checksum.
 *
 * XORs every payload byte into an accumulator initialized to zero.
 *
 * @param[in] payload Response payload bytes.
 * @param[in] payload_length Number of payload bytes to include.
 * @return XOR of all payload bytes, or zero for an empty payload.
 */
uint8_t i2c_probe_checksum(const uint8_t *payload, uint8_t payload_length) {
    uint8_t checksum = 0;
    for (uint8_t index = 0; index < payload_length; ++index) {
        checksum ^= payload[index];
    }
    return checksum;
}

/**
 * @brief Validates the final response from a probe exchange.
 *
 * Either nonzero status byte reports a status error. When checksum validation is enabled, the
 * payload XOR must equal the expected checksum. Disabled checksum validation accepts a response
 * whose status bytes are both zero.
 *
 * @param[in] response Final payload, checksum, and status fields.
 * @param[in] checksum_enabled True to compare the payload XOR with the expected checksum.
 * @return Valid, checksum-error, or status-error result.
 */
I2cProbeValidationResult i2c_probe_final_response_validate(const I2cProbeFinalResponse *response,
                                                           bool checksum_enabled) {
    if (response->primary_status != 0 || response->secondary_status != 0) {
        return I2C_PROBE_STATUS_ERROR;
    }
    if (checksum_enabled && i2c_probe_checksum(response->payload, response->payload_length) !=
                                response->expected_checksum) {
        return I2C_PROBE_CHECKSUM_ERROR;
    }
    return I2C_PROBE_VALID;
}
