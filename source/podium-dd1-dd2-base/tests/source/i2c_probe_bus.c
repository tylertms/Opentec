#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "i2c/probe.h"
#include "i2c/probe_bus.h"
#include "platform/aux_bus.h"

typedef enum {
    BUS_CALL_NONE,
    BUS_CALL_WRITE,
    BUS_CALL_READ,
} BusCall;

static BusCall bus_call;
static uint8_t bus_address;
static uint16_t bus_register;
static const uint8_t *bus_write_data;
static uint8_t *bus_read_data;
static uint16_t bus_length;
static bool bus_accepts;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    bus_call = BUS_CALL_WRITE;
    bus_address = address;
    bus_register = register_address;
    bus_write_data = data;
    bus_length = length;
    return bus_accepts;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    bus_call = BUS_CALL_READ;
    bus_address = address;
    bus_register = register_address;
    bus_read_data = data;
    bus_length = length;
    return bus_accepts;
}

static void reset_bus(void) {
    bus_call = BUS_CALL_NONE;
    bus_address = 0;
    bus_register = 0;
    bus_write_data = 0;
    bus_read_data = 0;
    bus_length = 0;
    bus_accepts = true;
}

static void test_starts_register_only_session_write(void) {
    reset_bus();
    assert(i2c_probe_bus_start(I2C_PROBE_BEGIN_SESSION, 0));
    assert(bus_call == BUS_CALL_WRITE);
    assert(bus_address == 0x48);
    assert(bus_register == 0x0f);
    assert(bus_write_data == 0);
    assert(bus_length == 0);
}

static void assert_read(I2cProbeCommand command, uint16_t selector, uint16_t length) {
    uint8_t response[0x1f];
    reset_bus();
    assert(i2c_probe_bus_start(command, response));
    assert(bus_call == BUS_CALL_READ);
    assert(bus_address == 0x48);
    assert(bus_register == selector);
    assert(bus_read_data == response);
    assert(bus_length == length);
}

static void test_starts_exact_response_reads(void) {
    assert_read(I2C_PROBE_READ_STARTUP_STATUS, 0x1f, 2);
    assert_read(I2C_PROBE_READ_SIGNATURE, 0x2f, 0x1f);
    assert_read(I2C_PROBE_READ_CONFIRMATION, 0xff, 2);
    assert_read(I2C_PROBE_READ_READY_STATUS, 0x07, 2);
}

static void test_rejects_invalid_requests(void) {
    reset_bus();
    assert(!i2c_probe_bus_start(I2C_PROBE_WRITE_CHUNK, 0));
    assert(bus_call == BUS_CALL_NONE);
    assert(!i2c_probe_bus_start(I2C_PROBE_READ_SIGNATURE, 0));
    assert(bus_call == BUS_CALL_NONE);
}

static void test_propagates_bus_backpressure(void) {
    uint8_t response[2];
    reset_bus();
    bus_accepts = false;
    assert(!i2c_probe_bus_start(I2C_PROBE_READ_STARTUP_STATUS, response));
    assert(bus_call == BUS_CALL_READ);
}

int main(void) {
    test_starts_register_only_session_write();
    test_starts_exact_response_reads();
    test_rejects_invalid_requests();
    test_propagates_bus_backpressure();
    return 0;
}
