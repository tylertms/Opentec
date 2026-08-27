#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/probe.h"
#include "platform/aux_bus.h"

static PlatformAuxBusStatus bus_status;
static uint8_t requested_address;
static uint16_t requested_register;
static uint8_t *requested_data;
static uint16_t requested_length;
static uint8_t start_count;

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }

    requested_address = address;
    requested_register = register_address;
    requested_data = data;
    requested_length = length;
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) { bus_status = PLATFORM_AUX_BUS_IDLE; }

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    requested_address = 0;
    requested_register = 0;
    requested_data = 0;
    requested_length = 0;
    start_count = 0;
}

static void finish_read(const uint8_t *data) {
    for (uint16_t index = 0; index < requested_length; index++) {
        requested_data[index] = data[index];
    }
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void test_discovers_motor(void) {
    MotorProbe probe;
    reset_bus();
    motor_probe_init(&probe);
    motor_probe_start(&probe);

    motor_probe_run(&probe);
    assert(requested_address == 0x78);
    assert(requested_register == 0);
    assert(requested_length == 1);

    const uint8_t status[1] = {0x95};
    finish_read(status);
    motor_probe_run(&probe);
    assert(requested_register == 1);
    assert(requested_length == 4);

    const uint8_t version[4] = {0x2a, 1, 2, 3};
    finish_read(version);
    motor_probe_run(&probe);

    const MotorIdentity *identity = motor_probe_identity(&probe);
    assert(identity != 0);
    assert(identity->protocol == MOTOR_PROTOCOL_POSITION_A);
    assert(identity->model == 5);
    assert(identity->version == UINT32_C(0x0302012a));
    assert(start_count == 2);
}

static void test_retries_failed_transfer(void) {
    MotorProbe probe;
    reset_bus();
    motor_probe_init(&probe);
    motor_probe_start(&probe);

    for (uint8_t failure = 0; failure < 3; failure++) {
        motor_probe_run(&probe);
        bus_status = PLATFORM_AUX_BUS_FAILED;
        motor_probe_run(&probe);
    }

    assert(probe.phase == MOTOR_PROBE_FAILED);
    assert(motor_probe_identity(&probe) == 0);
    assert(start_count == 3);
}

static void test_rejects_invalid_protocol(void) {
    MotorProbe probe;
    reset_bus();
    motor_probe_init(&probe);
    motor_probe_start(&probe);

    motor_probe_run(&probe);
    const uint8_t status[1] = {0x83};
    finish_read(status);
    motor_probe_run(&probe);
    const uint8_t version[4] = {0};
    finish_read(version);
    motor_probe_run(&probe);

    assert(probe.phase == MOTOR_PROBE_FAILED);
}

int main(void) {
    test_discovers_motor();
    test_retries_failed_transfer();
    test_rejects_invalid_protocol();
    return 0;
}
