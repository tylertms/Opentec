#include <assert.h>
#include <stdint.h>

#include "motor/identity.h"

static void test_legacy_identity(void) {
    const uint8_t version[4] = {0xc5, 0x12, 0x34, 0x56};
    MotorIdentity identity;

    assert(motor_identity_decode(0x21, version, &identity));
    assert(identity.protocol == MOTOR_PROTOCOL_LEGACY);
    assert(identity.model == 0);
    for (uint8_t index = 0; index < sizeof(version); index++) {
        assert(identity.version[index] == version[index]);
    }
    assert(identity.transfer_code == 5);
    assert(motor_identity_input_transfer_code(&identity) == 0);
    assert(identity.initial_status == 0x21);
    assert(!motor_identity_has_extended_parameters(&identity));
}

static void test_standard_identity(void) {
    const uint8_t version[4] = {0x2a, 0, 0, 0};
    MotorIdentity identity;

    assert(motor_identity_decode(0x94, version, &identity));
    assert(identity.protocol == MOTOR_PROTOCOL_STANDARD);
    assert(identity.model == 5);
    assert(identity.transfer_code == 0x2a);
    assert(motor_identity_input_transfer_code(&identity) == 0x2a);
    assert(!motor_identity_has_extended_parameters(&identity));
}

static void test_position_protocols(void) {
    const uint8_t version[4] = {1, 2, 3, 4};
    MotorIdentity identity;

    assert(motor_identity_decode(0xfd, version, &identity));
    assert(identity.protocol == MOTOR_PROTOCOL_POSITION_A);
    assert(identity.model == 0x1f);
    assert(motor_identity_input_transfer_code(&identity) == 1);
    assert(motor_identity_has_extended_parameters(&identity));

    assert(motor_identity_decode(0xfe, version, &identity));
    assert(identity.protocol == MOTOR_PROTOCOL_POSITION_B);
    assert(motor_identity_has_extended_parameters(&identity));
}

static void test_invalid_protocol(void) {
    const uint8_t version[4] = {0};
    MotorIdentity identity;

    assert(!motor_identity_decode(0x83, version, &identity));
    assert(motor_identity_input_transfer_code(NULL) == 0);
}

int main(void) {
    test_legacy_identity();
    test_standard_identity();
    test_position_protocols();
    test_invalid_protocol();
    return 0;
}
