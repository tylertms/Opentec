#include "i2c/probe_bus.h"

#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"
#include "platform/aux_bus.h"

enum {
    I2C_PROBE_DEVICE_ADDRESS = 0x48,
    I2C_PROBE_RESPONSE_REGISTER = 0x82,
};

/**
 * @brief Starts one fixed wheel-accessory probe transaction.
 *
 * Uses 7-bit device address 0x48 and the command selector as the register address. Session start
 * writes only the selector. The four response commands read their exact encoded response length.
 *
 * @param[in] command Fixed session or identification command to start.
 * @param[out] response Response destination for a read command, or null for session start.
 * @return True when the auxiliary bus accepted the transaction; otherwise false.
 */
bool i2c_probe_bus_start(I2cProbeCommand command, uint8_t *response) {
    const I2cProbeRequest *request = i2c_probe_request_lookup(command);
    if (request == 0) {
        return false;
    }
    if (request->response_length == 0) {
        return platform_aux_bus_start_write(I2C_PROBE_DEVICE_ADDRESS, request->selector, 0, 0);
    }
    if (response == 0) {
        return false;
    }
    return platform_aux_bus_start_read(I2C_PROBE_DEVICE_ADDRESS, request->selector, response,
                                       request->response_length);
}

/**
 * @brief Starts one encoded accessory transfer write.
 *
 * Addresses the accessory at 0x48, uses the encoded selector as the register, and transmits the
 * complete length-prefixed command body.
 *
 * @param[in] frame Encoded command selector, body, and body length.
 * @return True when the auxiliary bus accepted the write; otherwise false.
 */
bool i2c_probe_bus_start_frame_write(const I2cProbeTransferFrame *frame) {
    if (frame == 0 || frame->write_length == 0) {
        return false;
    }
    return platform_aux_bus_start_write(I2C_PROBE_DEVICE_ADDRESS, frame->selector,
                                        frame->write_data, frame->write_length);
}

/**
 * @brief Starts one encoded accessory transfer response read.
 *
 * Reads the response length selected by the command frame from accessory register 0x82.
 *
 * @param[in] frame Encoded command carrying the expected response length.
 * @param[out] response Destination for the complete response.
 * @return True when the auxiliary bus accepted the read; otherwise false.
 */
bool i2c_probe_bus_start_frame_read(const I2cProbeTransferFrame *frame, uint8_t *response) {
    if (frame == 0 || response == 0 || frame->response_length == 0) {
        return false;
    }
    return platform_aux_bus_start_read(I2C_PROBE_DEVICE_ADDRESS, I2C_PROBE_RESPONSE_REGISTER,
                                       response, frame->response_length);
}
