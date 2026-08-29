#include "common/motor/parameter.h"

enum {
    MOTOR_CONTROL_PARAMETER_FIRST = 32,
    MOTOR_CONTROL_PARAMETER_LAST = 42,
};

/**
 * @brief Reads one entry from the official sixty-four-entry motor parameter bank.
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
    if (index >= MOTOR_PARAMETER_COUNT || !bank->entries[index].writable || width == 0U ||
        width > bank->entries[index].width) {
        return false;
    }

    bank->entries[index].value = value;
    *control_settings_changed =
        index >= MOTOR_CONTROL_PARAMETER_FIRST && index <= MOTOR_CONTROL_PARAMETER_LAST;
    return true;
}
