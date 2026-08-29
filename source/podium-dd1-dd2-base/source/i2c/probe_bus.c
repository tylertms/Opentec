#include "i2c/probe_bus.h"

#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"
#include "platform/aux_bus.h"

enum {
    I2C_PROBE_DEVICE_ADDRESS = 0x48,
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
