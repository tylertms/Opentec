#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/aux_bus.h"
#include "secure_element/a71ch.h"
#include "secure_element/authentication.h"

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

static void complete_command_write(A71chAuthenticationService *service) {
    A71chAuthenticationStep step;
    assert(bus_call == BUS_CALL_WRITE);
    assert(a71ch_authentication_sequence_current(&service->sequence, &step));
    assert(bus_register == service->exchange.frame.selector);
    assert(bus_write_data == service->exchange.frame.write_data);
    assert(bus_length == service->exchange.frame.write_length);
    if (service->sequence.stage == A71CH_AUTHENTICATION_WRITING) {
        assert(memcmp(bus_write_data + 6, service->request + step.buffer_offset,
                      step.chunk_length) == 0);
    }
    ++command_writes;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void complete_response_read(A71chAuthenticationService *service, bool corrupt) {
    A71chAuthenticationStep step;
    A71chAuthenticationFrame *frame = &service->exchange.frame;
    assert(bus_call == BUS_CALL_READ);
    assert(bus_register == 0x82);
    assert(bus_length == frame->response_length);
    assert(a71ch_authentication_sequence_current(&service->sequence, &step));

    memset(bus_read_data, 0, bus_length);
    if (service->sequence.stage == A71CH_AUTHENTICATION_READING) {
        for (uint8_t index = 0; index < step.chunk_length; ++index) {
            bus_read_data[frame->response_payload_offset + index] =
                (uint8_t)(step.buffer_offset + index);
        }
        if (frame->response_integrity_length != 0) {
            uint8_t checksum = a71ch_lrc(bus_read_data + frame->response_integrity_offset,
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

static void drive_transfer(A71chAuthenticationService *service, bool corrupt_first_read) {
    bool corrupted = false;
    for (uint16_t iteration = 0; iteration < 1000 && a71ch_authentication_service_status(service) ==
                                                         A71CH_AUTHENTICATION_SERVICE_RUNNING;
         ++iteration) {
        a71ch_authentication_service_run(service);
        if (bus_status != PLATFORM_AUX_BUS_BUSY) {
            continue;
        }
        if (bus_call == BUS_CALL_WRITE) {
            complete_command_write(service);
        } else if (bus_register == 0x07) {
            complete_ready_read(0x07);
        } else {
            bool corrupt = corrupt_first_read && !corrupted &&
                           service->sequence.stage == A71CH_AUTHENTICATION_READING;
            complete_response_read(service, corrupt);
            corrupted |= corrupt;
        }
    }
}

static void test_completes_standard_transfer(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE];
    for (uint16_t index = 0; index < sizeof(request); ++index) {
        request[index] = (uint8_t)(index ^ 0x5a);
    }

    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), false));
    drive_transfer(&service, false);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(a71ch_authentication_service_result(&service) == A71CH_EXCHANGE_SUCCEEDED);
    assert(command_writes == 22);
    assert(response_reads == 22);
    uint16_t response_length = 0;
    const uint8_t *response = a71ch_authentication_service_response(&service, &response_length);
    assert(response == service.response);
    assert(response_length == A71CH_AUTHENTICATION_READ_SIZE);
    for (uint16_t index = 0; index < response_length; ++index) {
        assert(response[index] == (uint8_t)index);
    }
}

static void test_completes_checked_transfer(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), true));
    drive_transfer(&service, false);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 22);
    assert(response_reads == 22);
}

static void test_propagates_checked_response_failure(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), true));
    drive_transfer(&service, true);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_FAILED);
    assert(a71ch_authentication_service_result(&service) == A71CH_EXCHANGE_LRC_ERROR);
    assert(a71ch_authentication_service_response(&service, &(uint16_t){0}) == 0);
}

static void test_rejects_invalid_or_overlapping_start(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);

    assert(!a71ch_authentication_service_start(0, request, sizeof(request), false));
    assert(!a71ch_authentication_service_start(&service, 0, sizeof(request), false));
    assert(!a71ch_authentication_service_start(&service, request, sizeof(request) - 1, false));
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), false));
    assert(!a71ch_authentication_service_start(&service, request, sizeof(request), false));
}

int main(void) {
    test_completes_standard_transfer();
    test_completes_checked_transfer();
    test_propagates_checked_response_failure();
    test_rejects_invalid_or_overlapping_start();
    return 0;
}
