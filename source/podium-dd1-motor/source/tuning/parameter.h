#ifndef OPENTEC_MOTOR_PARAMETER_H
#define OPENTEC_MOTOR_PARAMETER_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of entries in the motor parameter bank. */
#define MOTOR_PARAMETER_COUNT 64U
/** @brief Size in bytes of an encoded parameter response. */
#define MOTOR_PARAMETER_RESPONSE_SIZE 5U
/** @brief Maximum size in bytes of an encoded parameter request. */
#define MOTOR_PARAMETER_REQUEST_SIZE 5U

/**
 * @brief Stores one motor parameter value and its access metadata.
 */
typedef struct {
    uint32_t value; /**< Zero-extended parameter value. */
    uint8_t width; /**< Declared number of bytes on the motor parameter wire. */
    bool writable; /**< True when the parameter accepts writes. */
} MotorParameter;

/**
 * @brief Stores all motor parameter values and access metadata.
 */
typedef struct {
    MotorParameter entries[MOTOR_PARAMETER_COUNT]; /**< Fixed parameter table entries. */
} MotorParameterBank;

/**
 * @brief Stores a parameter value and its encoded wire width.
 */
typedef struct {
    uint32_t value; /**< Zero-extended parameter value. */
    uint8_t width; /**< Number of value bytes in the parameter response. */
} MotorParameterResponse;

/**
 * @brief Initializes the motor parameter bank for the selected board identity.
 *
 * Undefined entries receive the invalid value and zero-width metadata; defined entries receive
 * their startup values, wire widths, and access modes.
 *
 * @param[out] bank Parameter bank to initialize.
 * @param[in] identity Board identity to publish in parameter entry zero.
 */
void motor_parameter_bank_initialize(MotorParameterBank *bank, uint8_t identity);

/**
 * @brief Reads one motor parameter entry.
 *
 * Valid indices return the current value and declared wire width, including undefined entries in
 * the fixed parameter table.
 *
 * @param[in] bank Parameter bank to read.
 * @param[in] index Parameter index to read.
 * @param[out] response Parameter value and wire width.
 * @return True when index identifies an entry in the parameter bank; otherwise false.
 */
bool motor_parameter_read(const MotorParameterBank *bank, uint8_t index,
                          MotorParameterResponse *response);

/**
 * @brief Writes one width-checked motor parameter entry.
 *
 * Only writable entries accept the value, and writes to live-control entries flag a settings
 * refresh for the runtime.
 *
 * @param[in,out] bank Parameter bank to update.
 * @param[in] index Parameter index to write.
 * @param[in] value Zero-extended parameter value.
 * @param[in] width Number of value bytes received.
 * @param[out] control_settings_changed Set when a live-control entry was written.
 * @return True when the value was accepted; otherwise false.
 */
bool motor_parameter_write(MotorParameterBank *bank, uint8_t index, uint32_t value, uint8_t width,
                           bool *control_settings_changed);

/**
 * @brief Encodes a motor parameter response into its wire buffer.
 *
 * The four-byte little-endian value is followed by the declared value width.
 *
 * @param[in] response Parameter value and wire width to encode.
 * @param[out] output Five-byte response buffer to populate.
 */
void motor_parameter_response_encode(const MotorParameterResponse *response,
                                     uint8_t output[MOTOR_PARAMETER_RESPONSE_SIZE]);

/**
 * @brief Applies one motor parameter request from its wire buffer.
 *
 * The first byte selects the parameter and the remaining zero through four bytes provide its
 * little-endian value.
 *
 * @param[in,out] bank Parameter bank to update.
 * @param[in] input Request buffer containing the parameter index and value.
 * @param[in] received_size Number of request bytes received, including the index.
 * @param[out] control_settings_changed Set when a live-control entry was written.
 * @return True when the request was valid and accepted; otherwise false.
 */
bool motor_parameter_request_apply(MotorParameterBank *bank,
                                   const uint8_t input[MOTOR_PARAMETER_REQUEST_SIZE],
                                   uint8_t received_size, bool *control_settings_changed);

#endif
