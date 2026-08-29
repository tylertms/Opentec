#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/device_identity.h"

typedef struct {
    uint8_t response;
    uint8_t submissions;
    I2cTransactionRequest requests[2];
} DriverState;

static uint8_t submit(void *context, I2cTransactionRequest request) {
    DriverState *state = context;
    state->requests[state->submissions++] = request;
    return 0;
}

static uint8_t poll(void *context) {
    DriverState *state = context;
    I2cTransactionRequest *request = &state->requests[state->submissions - 1];
    if (request->length == 1) {
        request->data[0] = state->response;
    } else {
        const uint8_t descriptor[] = {1, 2, 3, 4};
        memcpy(request->data, descriptor, sizeof(descriptor));
    }
    return 0;
}

static void recover(void *context) { (void)context; }

static I2cTransactionDriver driver(DriverState *state) {
    return (I2cTransactionDriver){
        .context = state,
        .submit = submit,
        .poll = poll,
        .recover = recover,
    };
}

static void test_decodes_direct_identifiers(void) {
    I2cDeviceIdentity identity;

    assert(i2c_device_identity_decode(0x37, &identity));
    assert(identity.device_class == I2C_DEVICE_CLASS_DIRECT);
    assert(identity.raw_code == 0x37);
    assert(identity.model_code == 0);
    assert(identity.subtype_code == 0);
}

static void test_decodes_every_supported_encoded_subtype(void) {
    I2cDeviceIdentity identity;

    assert(i2c_device_identity_decode(0xd4, &identity));
    assert(identity.device_class == I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ZERO);
    assert(identity.model_code == 0x15);
    assert(identity.subtype_code == 0);

    assert(i2c_device_identity_decode(0xd5, &identity));
    assert(identity.device_class == I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ONE_OR_TWO);
    assert(identity.model_code == 0x15);
    assert(identity.subtype_code == 1);

    assert(i2c_device_identity_decode(0xd6, &identity));
    assert(identity.device_class == I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ONE_OR_TWO);
    assert(identity.model_code == 0x15);
    assert(identity.subtype_code == 2);
}

static void test_rejects_encoded_subtype_three(void) {
    I2cDeviceIdentity identity;

    assert(!i2c_device_identity_decode(0xd7, &identity));
    assert(identity.device_class == I2C_DEVICE_CLASS_NONE);
    assert(identity.raw_code == 0xd7);
    assert(identity.model_code == 0x15);
    assert(identity.subtype_code == 3);
}

static void test_reads_primary_and_descriptor_before_classification(void) {
    DriverState state = {.response = 0xd5};
    I2cTransactionDriver operations = driver(&state);
    I2cTransaction transaction;
    I2cDeviceIdentification identification;
    i2c_transaction_init(&transaction);
    i2c_device_identification_init(&identification);

    assert(i2c_device_identification_service(&identification, &transaction, &operations) ==
           I2C_DEVICE_IDENTIFICATION_PENDING);
    assert(state.requests[0].address == 0xf0);
    assert(state.requests[0].length == 1);
    assert(state.requests[0].read);
    assert(i2c_device_identification_service(&identification, &transaction, &operations) ==
           I2C_DEVICE_IDENTIFICATION_PENDING);
    assert(identification.phase == I2C_DEVICE_IDENTIFICATION_DESCRIPTOR);

    assert(i2c_device_identification_service(&identification, &transaction, &operations) ==
           I2C_DEVICE_IDENTIFICATION_PENDING);
    assert(state.requests[1].address == 0xf0);
    assert(state.requests[1].length == 4);
    assert(state.requests[1].read);
    assert(i2c_device_identification_service(&identification, &transaction, &operations) ==
           I2C_DEVICE_IDENTIFICATION_READY);
    assert(identification.phase == I2C_DEVICE_IDENTIFICATION_PRIMARY);
    assert(identification.identity.device_class == I2C_DEVICE_CLASS_ENCODED_SUBTYPE_ONE_OR_TWO);
    assert(identification.descriptor[0] == 1);
    assert(identification.descriptor[3] == 4);
}

int main(void) {
    test_decodes_direct_identifiers();
    test_decodes_every_supported_encoded_subtype();
    test_rejects_encoded_subtype_three();
    test_reads_primary_and_descriptor_before_classification();
    return 0;
}
