#include "common/motor/parameter.h"

enum {
    MOTOR_CONTROL_PARAMETER_FIRST = 32,
    MOTOR_CONTROL_PARAMETER_LAST = 42,
};

/**
 * @brief Initializes the official sixty-four-entry motor parameter bank.
 *
 * Defined entries receive their wire widths, access modes, and startup values. All remaining
 * indices use the invalid value and zero-width metadata found in both product images.
 *
 * @param bank Parameter bank to initialize.
 * @param identity Runtime board identity published at index zero.
 */
void motor_parameter_bank_initialize(MotorParameterBank *bank, uint8_t identity) {
    for (uint32_t index = 0U; index < MOTOR_PARAMETER_COUNT; ++index) {
        bank->entries[index] = (MotorParameter){.value = UINT32_MAX};
    }

    bank->entries[0] = (MotorParameter){.value = identity, .width = 1U};
    bank->entries[1] = (MotorParameter){.value = UINT32_C(0x01010003), .width = 4U};
    bank->entries[3] = (MotorParameter){.width = 2U, .writable = true};
    bank->entries[4] = (MotorParameter){.value = 0xaaU, .width = 1U, .writable = true};
    bank->entries[5] = (MotorParameter){.width = 2U, .writable = true};
    bank->entries[6] = (MotorParameter){.width = 2U, .writable = true};
    bank->entries[7] = (MotorParameter){.width = 1U};
    bank->entries[8] = (MotorParameter){.width = 1U};
    bank->entries[16] = (MotorParameter){.width = 2U};
    bank->entries[17] = (MotorParameter){.width = 4U};
    bank->entries[18] = (MotorParameter){.width = 2U};
    bank->entries[19] = (MotorParameter){.width = 2U};
    bank->entries[20] = (MotorParameter){.width = 2U};
    bank->entries[32] = (MotorParameter){.value = 0xedU, .width = 1U, .writable = true};
    bank->entries[33] = (MotorParameter){.value = 0x23U, .width = 1U, .writable = true};
    bank->entries[34] = (MotorParameter){.width = 1U, .writable = true};
    bank->entries[35] = (MotorParameter){.value = 0xffU, .width = 1U, .writable = true};
    bank->entries[36] = (MotorParameter){.width = 2U, .writable = true};
    bank->entries[37] = (MotorParameter){.width = 1U, .writable = true};
    bank->entries[38] = (MotorParameter){.value = 6U, .width = 1U, .writable = true};
    for (uint32_t index = 39U; index <= MOTOR_CONTROL_PARAMETER_LAST; ++index) {
        bank->entries[index] = (MotorParameter){.value = 100U, .width = 1U, .writable = true};
    }
}

/**
 * @brief Reads one entry from the official sixty-four-entry motor parameter bank.
 *
 * Valid indices return their current value and declared transaction width, including undefined
 * zero-width entries inside the fixed table.
 *
 * @param bank Motor parameter values and access metadata.
 * @param index Parameter index requested by the base firmware.
 * @param response Parameter value and wire width.
 * @return True when the index is valid.
 */
bool motor_parameter_read(const MotorParameterBank *bank, uint8_t index,
                          MotorParameterResponse *response) {
    if (index >= MOTOR_PARAMETER_COUNT) {
        return false;
    }

    response->value = bank->entries[index].value;
    response->width = bank->entries[index].width;
    return true;
}

/**
 * @brief Applies one width-checked write to the official motor parameter bank.
 *
 * Only writable entries accept values, and live-control indices request a settings refresh.
 *
 * @param bank Motor parameter values and access metadata.
 * @param index Parameter index supplied by the base firmware.
 * @param value Zero-extended little-endian parameter value.
 * @param width Number of received value bytes.
 * @param control_settings_changed Set when indices 32 through 42 require control-state refresh.
 * @return True when the parameter accepted the write.
 */
bool motor_parameter_write(MotorParameterBank *bank, uint8_t index, uint32_t value, uint8_t width,
                           bool *control_settings_changed) {
    *control_settings_changed = false;
    if (index >= MOTOR_PARAMETER_COUNT || !bank->entries[index].writable ||
        width > bank->entries[index].width) {
        return false;
    }

    bank->entries[index].value = value;
    *control_settings_changed =
        index >= MOTOR_CONTROL_PARAMETER_FIRST && index <= MOTOR_CONTROL_PARAMETER_LAST;
    return true;
}

/**
 * @brief Encodes one parameter response for the motor I2C transport.
 *
 * The response contains the little-endian value followed by its declared width.
 *
 * @param response Parameter value and declared wire width.
 * @param output Five-byte I2C response buffer.
 */
void motor_parameter_response_encode(const MotorParameterResponse *response,
                                     uint8_t output[MOTOR_PARAMETER_RESPONSE_SIZE]) {
    output[0] = (uint8_t)response->value;
    output[1] = (uint8_t)(response->value >> 8U);
    output[2] = (uint8_t)(response->value >> 16U);
    output[3] = (uint8_t)(response->value >> 24U);
    output[4] = response->width;
}

/**
 * @brief Applies one complete parameter write received over the motor I2C transport.
 *
 * The request contains a parameter index followed by zero to four little-endian value bytes.
 *
 * @param bank Parameter bank to update.
 * @param input Five-byte receive buffer containing the index and value.
 * @param received_size Number of bytes received, including the index.
 * @param control_settings_changed Set when the write changes a live control parameter.
 * @return True when a complete, writable parameter request was accepted.
 */
bool motor_parameter_request_apply(MotorParameterBank *bank,
                                   const uint8_t input[MOTOR_PARAMETER_REQUEST_SIZE],
                                   uint8_t received_size, bool *control_settings_changed) {
    if (received_size == 0U || received_size > MOTOR_PARAMETER_REQUEST_SIZE) {
        *control_settings_changed = false;
        return false;
    }

    uint32_t value = 0U;
    uint8_t width = received_size - 1U;
    for (uint8_t index = 0U; index < width; ++index) {
        value |= (uint32_t)input[index + 1U] << (index * 8U);
    }
    return motor_parameter_write(bank, input[0], value, width, control_settings_changed);
}
