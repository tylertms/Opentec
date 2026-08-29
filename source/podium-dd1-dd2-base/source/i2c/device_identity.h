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

void i2c_device_identification_init(I2cDeviceIdentification *identification);
bool i2c_device_identity_decode(uint8_t response, I2cDeviceIdentity *identity);
I2cDeviceIdentificationResult
i2c_device_identification_service(I2cDeviceIdentification *identification,
                                  I2cTransaction *transaction, const I2cTransactionDriver *driver);

#endif
