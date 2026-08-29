#include "i2c/device_identity.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    I2C_IDENTIFICATION_ADDRESS = 0xf0,
    I2C_IDENTIFICATION_MARKER = 0x80,
    I2C_IDENTIFICATION_SUBTYPE_MASK = 0x03,
    I2C_IDENTIFICATION_MODEL_MASK = 0x7c,
    I2C_IDENTIFICATION_MODEL_SHIFT = 2,
};

/**
 * @brief Initializes I2C device identification.
 *
 * Starts with the one-byte primary response and clears the retained descriptor and identity.
 *
 * @param[out] identification Identification exchange to initialize.
 */
void i2c_device_identification_init(I2cDeviceIdentification *identification) {
    *identification = (I2cDeviceIdentification){0};
}

/**
 * @brief Classifies an I2C peripheral identification response.
 *
 * Responses without bit 7 set use class 1 and retain the complete response as their direct code.
 * Encoded responses use bits 2 through 6 as a five-bit model. Subtype 0 selects class 2, subtypes
 * 1 and 2 select class 3, and subtype 3 is rejected.
 *
 * @param[in] response One-byte identification response from I2C address 0xF0.
 * @param[out] identity Decoded class, raw response, model, and subtype.
 * @return True when the response selects a supported device class.
 */
bool i2c_device_identity_decode(uint8_t response, I2cDeviceIdentity *identity) {
    I2cDeviceIdentity decoded = {
        .raw_code = response,
    };

    if ((response & I2C_IDENTIFICATION_MARKER) == 0) {
        decoded.device_class = I2C_DEVICE_CLASS_DIRECT;
        *identity = decoded;
        return true;
    }

    decoded.subtype_code = response & I2C_IDENTIFICATION_SUBTYPE_MASK;
    decoded.model_code =
        (response & I2C_IDENTIFICATION_MODEL_MASK) >> I2C_IDENTIFICATION_MODEL_SHIFT;
    if (decoded.subtype_code == 0) {
        decoded.device_class = I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ZERO;
    } else if (decoded.subtype_code <= 2) {
        decoded.device_class = I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ONE_OR_TWO;
    } else {
        *identity = decoded;
        return false;
    }

    *identity = decoded;
    return true;
}

/**
 * @brief Services the two-stage I2C identification exchange.
 *
 * Reads one primary byte and then four descriptor bytes from address 0xF0. A completed descriptor
 * read classifies the primary byte and restarts the exchange at its first phase. Pending and
 * recovered channel states preserve the active phase for retry.
 *
 * @param[in,out] identification Persistent exchange buffers, phase, and decoded identity.
 * @param[in,out] transaction Asynchronous I2C transaction channel.
 * @param[in] driver I2C submission, completion-poll, and recovery operations.
 * @return Pending while either read is active, ready for a supported identity, or invalid.
 */
I2cDeviceIdentificationResult
i2c_device_identification_service(I2cDeviceIdentification *identification,
                                  I2cTransaction *transaction, const I2cTransactionDriver *driver) {
    I2cTransactionRequest request = {
        .address = I2C_IDENTIFICATION_ADDRESS,
        .data = identification->phase == I2C_DEVICE_IDENTIFICATION_PRIMARY
                    ? &identification->primary_response
                    : identification->descriptor,
        .length = identification->phase == I2C_DEVICE_IDENTIFICATION_PRIMARY
                      ? 1
                      : sizeof(identification->descriptor),
        .read = true,
    };

    if (i2c_transaction_service(transaction, driver, request) != I2C_TRANSACTION_COMPLETE) {
        return I2C_DEVICE_IDENTIFICATION_PENDING;
    }
    if (identification->phase == I2C_DEVICE_IDENTIFICATION_PRIMARY) {
        identification->phase = I2C_DEVICE_IDENTIFICATION_DESCRIPTOR;
        return I2C_DEVICE_IDENTIFICATION_PENDING;
    }

    identification->phase = I2C_DEVICE_IDENTIFICATION_PRIMARY;
    return i2c_device_identity_decode(identification->primary_response, &identification->identity)
               ? I2C_DEVICE_IDENTIFICATION_READY
               : I2C_DEVICE_IDENTIFICATION_INVALID;
}
