#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/probe.h"
#include "i2c/probe_transfer_service.h"
#include "platform/aux_bus.h"

typedef enum {
    BUS_CALL_NONE,
    BUS_CALL_WRITE,
    BUS_CALL_READ,
} BusCall;

static BusCall bus_call;
static PlatformAuxBusStatus bus_status;
static uint16_t bus_register;
static const uint8_t *bus_write_data;
static uint8_t *bus_read_data;
static uint16_t bus_length;
static uint8_t command_writes;
static uint8_t response_reads;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    assert(address == 0x48);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
    bus_call = BUS_CALL_WRITE;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    bus_register = register_address;
    bus_write_data = data;
    bus_length = length;
    return true;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    assert(address == 0x48);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
    bus_call = BUS_CALL_READ;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    bus_register = register_address;
    bus_read_data = data;
    bus_length = length;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) {
    bus_call = BUS_CALL_NONE;
    bus_status = PLATFORM_AUX_BUS_IDLE;
}

static void reset_bus(void) {
    bus_call = BUS_CALL_NONE;
    bus_status = PLATFORM_AUX_BUS_IDLE;
    bus_register = 0;
    bus_write_data = 0;
    bus_read_data = 0;
    bus_length = 0;
    command_writes = 0;
    response_reads = 0;
}

static void complete_ready_read(uint8_t status) {
    assert(bus_call == BUS_CALL_READ);
    assert(bus_register == 0x07);
    assert(bus_length == 2);
    bus_read_data[1] = status;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void complete_command_write(I2cProbeTransferService *service) {
    I2cProbeTransferStep step;
    assert(bus_call == BUS_CALL_WRITE);
    assert(i2c_probe_transfer_sequence_current(&service->sequence, &step));
    assert(bus_register == service->exchange.frame.selector);
    assert(bus_write_data == service->exchange.frame.write_data);
    assert(bus_length == service->exchange.frame.write_length);
    if (service->sequence.stage == I2C_PROBE_TRANSFER_WRITING) {
        assert(memcmp(bus_write_data + 6, service->request + step.buffer_offset,
                      step.chunk_length) == 0);
    }
    ++command_writes;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void complete_response_read(I2cProbeTransferService *service, bool corrupt) {
    I2cProbeTransferStep step;
    I2cProbeTransferFrame *frame = &service->exchange.frame;
    assert(bus_call == BUS_CALL_READ);
    assert(bus_register == 0x82);
    assert(bus_length == frame->response_length);
    assert(i2c_probe_transfer_sequence_current(&service->sequence, &step));

    memset(bus_read_data, 0, bus_length);
    if (service->sequence.stage == I2C_PROBE_TRANSFER_READING) {
        for (uint8_t index = 0; index < step.chunk_length; ++index) {
            bus_read_data[frame->response_payload_offset + index] =
                (uint8_t)(step.buffer_offset + index);
        }
        if (frame->response_integrity_length != 0) {
            uint8_t checksum = i2c_probe_checksum(bus_read_data + frame->response_integrity_offset,
                                                  frame->response_integrity_length);
            bus_read_data[frame->response_integrity_offset + frame->response_integrity_length - 1] =
                checksum;
            if (corrupt) {
                bus_read_data[frame->response_integrity_offset] ^= 1;
            }
        }
    }
    ++response_reads;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void drive_transfer(I2cProbeTransferService *service, bool corrupt_first_read) {
    bool corrupted = false;
    for (uint16_t iteration = 0; iteration < 1000 && i2c_probe_transfer_service_status(service) ==
                                                         I2C_PROBE_TRANSFER_SERVICE_RUNNING;
         ++iteration) {
        i2c_probe_transfer_service_run(service);
        if (bus_status != PLATFORM_AUX_BUS_BUSY) {
            continue;
        }
        if (bus_call == BUS_CALL_WRITE) {
            complete_command_write(service);
        } else if (bus_register == 0x07) {
            complete_ready_read(0x07);
        } else {
            bool corrupt = corrupt_first_read && !corrupted &&
                           service->sequence.stage == I2C_PROBE_TRANSFER_READING;
            complete_response_read(service, corrupt);
            corrupted |= corrupt;
        }
    }
}

static void test_completes_standard_transfer(void) {
    uint8_t request[I2C_PROBE_TRANSFER_WRITE_SIZE];
    for (uint16_t index = 0; index < sizeof(request); ++index) {
        request[index] = (uint8_t)(index ^ 0x5a);
    }

    reset_bus();
    I2cProbeTransferService service;
    i2c_probe_transfer_service_init(&service);
    assert(i2c_probe_transfer_service_start(&service, request, sizeof(request), false));
    drive_transfer(&service, false);

    assert(i2c_probe_transfer_service_status(&service) == I2C_PROBE_TRANSFER_SERVICE_COMPLETE);
    assert(i2c_probe_transfer_service_result(&service) == I2C_PROBE_EXCHANGE_SUCCEEDED);
    assert(command_writes == 22);
    assert(response_reads == 22);
    uint16_t response_length = 0;
    const uint8_t *response = i2c_probe_transfer_service_response(&service, &response_length);
    assert(response == service.response);
    assert(response_length == I2C_PROBE_TRANSFER_READ_SIZE);
    for (uint16_t index = 0; index < response_length; ++index) {
        assert(response[index] == (uint8_t)index);
    }
}

static void test_completes_checked_transfer(void) {
    uint8_t request[I2C_PROBE_TRANSFER_WRITE_SIZE] = {0};
    reset_bus();
    I2cProbeTransferService service;
    i2c_probe_transfer_service_init(&service);
    assert(i2c_probe_transfer_service_start(&service, request, sizeof(request), true));
    drive_transfer(&service, false);

    assert(i2c_probe_transfer_service_status(&service) == I2C_PROBE_TRANSFER_SERVICE_COMPLETE);
    assert(command_writes == 22);
    assert(response_reads == 22);
}

static void test_propagates_checked_response_failure(void) {
    uint8_t request[I2C_PROBE_TRANSFER_WRITE_SIZE] = {0};
    reset_bus();
    I2cProbeTransferService service;
    i2c_probe_transfer_service_init(&service);
    assert(i2c_probe_transfer_service_start(&service, request, sizeof(request), true));
    drive_transfer(&service, true);

    assert(i2c_probe_transfer_service_status(&service) == I2C_PROBE_TRANSFER_SERVICE_FAILED);
    assert(i2c_probe_transfer_service_result(&service) == I2C_PROBE_EXCHANGE_CHECKSUM_ERROR);
    assert(i2c_probe_transfer_service_response(&service, &(uint16_t){0}) == 0);
}

static void test_rejects_invalid_or_overlapping_start(void) {
    uint8_t request[I2C_PROBE_TRANSFER_WRITE_SIZE] = {0};
    I2cProbeTransferService service;
    i2c_probe_transfer_service_init(&service);

    assert(!i2c_probe_transfer_service_start(0, request, sizeof(request), false));
    assert(!i2c_probe_transfer_service_start(&service, 0, sizeof(request), false));
    assert(!i2c_probe_transfer_service_start(&service, request, sizeof(request) - 1, false));
    assert(i2c_probe_transfer_service_start(&service, request, sizeof(request), false));
    assert(!i2c_probe_transfer_service_start(&service, request, sizeof(request), false));
}

int main(void) {
    test_completes_standard_transfer();
    test_completes_checked_transfer();
    test_propagates_checked_response_failure();
    test_rejects_invalid_or_overlapping_start();
    return 0;
}
