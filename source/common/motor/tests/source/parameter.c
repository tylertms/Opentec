#include "common/motor/parameter.h"

#include <assert.h>

static void test_read(void) {
    MotorParameterBank bank = {0};
    bank.entries[12] = (MotorParameter){
        .value = 0x78563412U,
        .width = 4U,
        .writable = true,
    };

    MotorParameterResponse response;
    assert(motor_parameter_read(&bank, 12U, &response));
    assert(response.value == 0x78563412U);
    assert(response.width == 4U);
    assert(!motor_parameter_read(&bank, MOTOR_PARAMETER_COUNT, &response));
}

static void test_write(void) {
    MotorParameterBank bank = {0};
    bank.entries[12] = (MotorParameter){.width = 2U, .writable = true};
    bank.entries[31] = (MotorParameter){.width = 4U, .writable = true};
    bank.entries[32] = (MotorParameter){.width = 4U, .writable = true};
    bank.entries[42] = (MotorParameter){.width = 4U, .writable = true};
    bank.entries[43] = (MotorParameter){.width = 4U, .writable = true};

    bool changed;
    assert(motor_parameter_write(&bank, 12U, 0x1234U, 2U, &changed));
    assert(bank.entries[12].value == 0x1234U);
    assert(!changed);
    assert(!motor_parameter_write(&bank, 12U, 0x123456U, 3U, &changed));
    assert(!motor_parameter_write(&bank, 13U, 1U, 1U, &changed));
    assert(!motor_parameter_write(&bank, MOTOR_PARAMETER_COUNT, 1U, 1U, &changed));

    assert(motor_parameter_write(&bank, 31U, 1U, 4U, &changed));
    assert(!changed);
    assert(motor_parameter_write(&bank, 32U, 1U, 4U, &changed));
    assert(changed);
    assert(motor_parameter_write(&bank, 42U, 1U, 4U, &changed));
    assert(changed);
    assert(motor_parameter_write(&bank, 43U, 1U, 4U, &changed));
    assert(!changed);
}

int main(void) {
    test_read();
    test_write();
    return 0;
}
