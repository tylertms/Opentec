#include "tuning/parameter.h"

#include <assert.h>

static void test_initialization(void) {
    MotorParameterBank bank;
    motor_parameter_bank_initialize(&bank, 0xd5U);

    assert(bank.entries[0].value == 0xd5U);
    assert(bank.entries[0].width == 1U);
    assert(!bank.entries[0].writable);
    assert(bank.entries[1].value == UINT32_C(0x01010003));
    assert(bank.entries[1].width == 4U);
    assert(bank.entries[2].value == UINT32_MAX);
    assert(bank.entries[2].width == 0U);
    assert(bank.entries[3].width == 2U);
    assert(bank.entries[3].writable);
    assert(bank.entries[4].value == 0xaaU);
    assert(bank.entries[4].writable);
    assert(bank.entries[8].value == 0U);
    assert(bank.entries[8].width == 1U);
    assert(!bank.entries[8].writable);
    assert(bank.entries[17].width == 4U);
    assert(!bank.entries[17].writable);
    assert(bank.entries[32].value == 0xedU);
    assert(bank.entries[33].value == 0x23U);
    assert(bank.entries[35].value == 0xffU);
    assert(bank.entries[36].width == 2U);
    assert(bank.entries[38].value == 6U);
    for (uint32_t index = 39U; index <= 42U; ++index) {
        assert(bank.entries[index].value == 100U);
        assert(bank.entries[index].width == 1U);
        assert(bank.entries[index].writable);
    }
    for (uint32_t index = 43U; index < MOTOR_PARAMETER_COUNT; ++index) {
        assert(bank.entries[index].value == UINT32_MAX);
        assert(bank.entries[index].width == 0U);
        assert(!bank.entries[index].writable);
    }
}

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

static void test_wire_format(void) {
    MotorParameterResponse response = {
        .value = UINT32_C(0x78563412),
        .width = 4U,
    };
    uint8_t response_bytes[MOTOR_PARAMETER_RESPONSE_SIZE];
    motor_parameter_response_encode(&response, response_bytes);
    assert(response_bytes[0] == 0x12U);
    assert(response_bytes[1] == 0x34U);
    assert(response_bytes[2] == 0x56U);
    assert(response_bytes[3] == 0x78U);
    assert(response_bytes[4] == 4U);

    response = (MotorParameterResponse){.value = UINT32_MAX, .width = 2U};
    motor_parameter_response_encode(&response, response_bytes);
    assert(response_bytes[0] == 0xffU);
    assert(response_bytes[1] == 0xffU);
    assert(response_bytes[2] == 0xffU);
    assert(response_bytes[3] == 0xffU);
    assert(response_bytes[4] == 2U);

    MotorParameterBank bank = {0};
    bank.entries[33] = (MotorParameter){.width = 4U, .writable = true};
    uint8_t request[MOTOR_PARAMETER_REQUEST_SIZE] = {33U, 0x78U, 0x56U, 0x34U, 0x12U};
    bool changed;
    assert(motor_parameter_request_apply(&bank, request, 5U, &changed));
    assert(bank.entries[33].value == UINT32_C(0x12345678));
    assert(changed);
    assert(motor_parameter_request_apply(&bank, request, 1U, &changed));
    assert(bank.entries[33].value == 0U);
    assert(changed);
    assert(!motor_parameter_request_apply(&bank, request, 0U, &changed));
    assert(!changed);
    assert(!motor_parameter_request_apply(&bank, request, MOTOR_PARAMETER_REQUEST_SIZE + 1U,
                                          &changed));
    assert(!changed);
}

int main(void) {
    test_initialization();
    test_read();
    test_write();
    test_wire_format();
    return 0;
}
