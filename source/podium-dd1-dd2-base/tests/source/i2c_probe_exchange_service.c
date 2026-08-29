#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/probe.h"
#include "i2c/probe_exchange_service.h"
#include "platform/aux_bus.h"

typedef enum {
    BUS_CALL_NONE,
    BUS_CALL_WRITE,
    BUS_CALL_READ,
} BusCall;

static BusCall bus_call;
static PlatformAuxBusStatus bus_status;
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
    if (bus_accepts) {
        bus_status = PLATFORM_AUX_BUS_BUSY;
    }
    return bus_accepts;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    bus_call = BUS_CALL_READ;
    bus_address = address;
    bus_register = register_address;
    bus_read_data = data;
    bus_length = length;
    if (bus_accepts) {
        bus_status = PLATFORM_AUX_BUS_BUSY;
    }
    return bus_accepts;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    bus_call = BUS_CALL_NONE;
}

static void reset_bus(void) {
    bus_call = BUS_CALL_NONE;
    bus_status = PLATFORM_AUX_BUS_IDLE;
    bus_address = 0;
    bus_register = 0;
    bus_write_data = 0;
    bus_read_data = 0;
    bus_length = 0;
    bus_accepts = true;
}

static I2cProbeTransferFrame make_frame(void) {
    static const uint8_t payload[] = {0x31, 0x32, 0x33};
    I2cProbeTransferInput input = {
        .phase = 2,
        .chunk_index = 1,
        .chunk = payload,
        .chunk_length = sizeof(payload),
    };
    I2cProbeTransferFrame frame;
    assert(i2c_probe_transfer_encode(I2C_PROBE_WRITE_CHUNK, &input, &frame));
    return frame;
}

static I2cProbeTransferFrame make_read_frame(bool checked) {
    I2cProbeTransferInput input = {
        .phase = 2,
        .chunk_index = 1,
        .chunk_length = 3,
    };
    I2cProbeTransferFrame frame;
    assert(i2c_probe_transfer_encode(checked ? I2C_PROBE_READ_CHECKED_CHUNK : I2C_PROBE_READ_CHUNK,
                                     &input, &frame));
    return frame;
}

static void complete_read(uint8_t status) {
    assert(bus_call == BUS_CALL_READ);
    bus_read_data[1] = status;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void complete_operation(void) { bus_status = PLATFORM_AUX_BUS_SUCCEEDED; }

static void advance_to_response(I2cProbeExchangeService *service) {
    i2c_probe_exchange_service_run(service);
    assert(bus_register == 0x07);
    assert(bus_length == 2);
    complete_read(0x07);
    i2c_probe_exchange_service_run(service);

    i2c_probe_exchange_service_run(service);
    assert(bus_call == BUS_CALL_WRITE);
    assert(bus_address == 0x48);
    assert(bus_register == service->frame.selector);
    assert(bus_write_data == service->frame.write_data);
    assert(bus_length == service->frame.write_length);
    complete_operation();
    i2c_probe_exchange_service_run(service);

    i2c_probe_exchange_service_run(service);
    assert(bus_register == 0x07);
    complete_read(0x07);
    i2c_probe_exchange_service_run(service);

    i2c_probe_exchange_service_run(service);
    assert(bus_call == BUS_CALL_READ);
    assert(bus_register == 0x82);
    assert(bus_length == service->frame.response_length);
}

static void test_completes_exchange_and_exposes_payload(void) {
    reset_bus();
    I2cProbeExchangeService service;
    I2cProbeTransferFrame frame = make_read_frame(false);
    i2c_probe_exchange_service_init(&service);
    assert(i2c_probe_exchange_service_start(&service, &frame));

    advance_to_response(&service);
    bus_read_data[0] = 0x84;
    bus_read_data[1] = 0x91;
    bus_read_data[2] = 0x31;
    bus_read_data[3] = 0x32;
    bus_read_data[4] = 0x33;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    i2c_probe_exchange_service_run(&service);

    assert(i2c_probe_exchange_service_status(&service) == I2C_PROBE_EXCHANGE_SERVICE_COMPLETE);
    assert(i2c_probe_exchange_service_result(&service) == I2C_PROBE_EXCHANGE_SUCCEEDED);
    uint8_t length = 0;
    const uint8_t *payload = i2c_probe_exchange_service_payload(&service, &length);
    assert(payload == service.response + 2);
    assert(length == 3);
    assert(memcmp(payload, "123", length) == 0);
}

static void test_waits_through_busy_statuses(void) {
    reset_bus();
    I2cProbeExchangeService service;
    I2cProbeTransferFrame frame = make_frame();
    i2c_probe_exchange_service_init(&service);
    assert(i2c_probe_exchange_service_start(&service, &frame));

    i2c_probe_exchange_service_run(&service);
    complete_read(0x17);
    i2c_probe_exchange_service_run(&service);
    assert(service.exchange.stage == I2C_PROBE_EXCHANGE_WAIT_READY);
    i2c_probe_exchange_service_run(&service);
    assert(bus_register == 0x07);
}

static void test_retries_failed_bus_operation(void) {
    reset_bus();
    I2cProbeExchangeService service;
    I2cProbeTransferFrame frame = make_frame();
    i2c_probe_exchange_service_init(&service);
    assert(i2c_probe_exchange_service_start(&service, &frame));

    i2c_probe_exchange_service_run(&service);
    bus_status = PLATFORM_AUX_BUS_FAILED;
    i2c_probe_exchange_service_run(&service);
    assert(service.exchange.stage == I2C_PROBE_EXCHANGE_WAIT_READY);
    assert(!service.transfer_active);
    i2c_probe_exchange_service_run(&service);
    assert(bus_call == BUS_CALL_READ);
    assert(bus_register == 0x07);
}

static void test_classifies_command_and_checksum_failures(void) {
    reset_bus();
    I2cProbeExchangeService service;
    I2cProbeTransferFrame frame = make_frame();
    i2c_probe_exchange_service_init(&service);
    assert(i2c_probe_exchange_service_start(&service, &frame));

    for (uint8_t attempt = 0; attempt < 4; ++attempt) {
        i2c_probe_exchange_service_run(&service);
        complete_read(0x41);
        i2c_probe_exchange_service_run(&service);
    }
    assert(i2c_probe_exchange_service_status(&service) == I2C_PROBE_EXCHANGE_SERVICE_FAILED);
    assert(i2c_probe_exchange_service_result(&service) == I2C_PROBE_EXCHANGE_COMMAND_ERROR);

    frame = make_read_frame(true);
    assert(i2c_probe_exchange_service_start(&service, &frame));
    advance_to_response(&service);
    bus_read_data[2] = 1;
    complete_operation();
    i2c_probe_exchange_service_run(&service);
    assert(i2c_probe_exchange_service_status(&service) == I2C_PROBE_EXCHANGE_SERVICE_FAILED);
    assert(i2c_probe_exchange_service_result(&service) == I2C_PROBE_EXCHANGE_CHECKSUM_ERROR);
}

static void test_rejects_invalid_or_overlapping_starts(void) {
    reset_bus();
    I2cProbeExchangeService service;
    I2cProbeTransferFrame frame = make_frame();
    i2c_probe_exchange_service_init(&service);

    assert(!i2c_probe_exchange_service_start(0, &frame));
    assert(!i2c_probe_exchange_service_start(&service, 0));
    frame.write_length = 0;
    assert(!i2c_probe_exchange_service_start(&service, &frame));
    frame = make_frame();
    assert(i2c_probe_exchange_service_start(&service, &frame));
    assert(!i2c_probe_exchange_service_start(&service, &frame));
    assert(i2c_probe_exchange_service_payload(&service, &frame.response_length) == 0);
}

int main(void) {
    test_completes_exchange_and_exposes_payload();
    test_waits_through_busy_statuses();
    test_retries_failed_bus_operation();
    test_classifies_command_and_checksum_failures();
    test_rejects_invalid_or_overlapping_starts();
    return 0;
}
