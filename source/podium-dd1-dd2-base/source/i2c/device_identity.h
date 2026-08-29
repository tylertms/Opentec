#ifndef OPENTEC_BASE_I2C_DEVICE_IDENTITY_H
#define OPENTEC_BASE_I2C_DEVICE_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c/transaction.h"

typedef enum {
    I2C_DEVICE_CLASS_NONE,
    I2C_DEVICE_CLASS_DIRECT,
    I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ZERO,
    I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ONE_OR_TWO,
} I2cDeviceClass;

typedef struct {
    I2cDeviceClass device_class;
    uint8_t raw_code;
    uint8_t model_code;
    uint8_t subtype_code;
} I2cDeviceIdentity;

typedef enum {
    I2C_DEVICE_IDENTIFICATION_PRIMARY,
    I2C_DEVICE_IDENTIFICATION_DESCRIPTOR,
} I2cDeviceIdentificationPhase;

typedef enum {
    I2C_DEVICE_IDENTIFICATION_PENDING,
    I2C_DEVICE_IDENTIFICATION_READY,
    I2C_DEVICE_IDENTIFICATION_INVALID,
} I2cDeviceIdentificationResult;

typedef struct {
    I2cDeviceIdentificationPhase phase;
    uint8_t primary_response;
    uint8_t descriptor[4];
    I2cDeviceIdentity identity;
} I2cDeviceIdentification;

/**
 * @brief Initializes I2C device identification.
 *
 * Starts with the one-byte primary response and clears the retained descriptor and identity.
 *
 * @param[out] identification Identification exchange to initialize.
 */
void i2c_device_identification_init(I2cDeviceIdentification *identification);

/**
 * @brief Classifies an I2C peripheral identification response.
 *
 * Decodes direct identifiers and the supported encoded model and subtype combinations.
 *
 * @param[in] response One-byte identification response from channel 0xF0.
 * @param[out] identity Decoded class, raw response, model, and subtype.
 * @return True when the response selects a supported device class.
 */
bool i2c_device_identity_decode(uint8_t response, I2cDeviceIdentity *identity);

/**
 * @brief Services the two-stage I2C identification exchange.
 *
 * Reads the primary response from parameter 0 and its descriptor from parameter 1 before
 * classifying the attached peripheral.
 *
 * @param[in,out] identification Persistent exchange buffers, phase, and decoded identity.
 * @param[in,out] transaction Asynchronous I2C transaction channel.
 * @param[in] driver Transfer submission, completion-poll, and recovery operations.
 * @return Pending while either read is active, ready for a supported identity, or invalid.
 */
I2cDeviceIdentificationResult
i2c_device_identification_service(I2cDeviceIdentification *identification,
                                  I2cTransaction *transaction, const I2cTransactionDriver *driver);

#endif
