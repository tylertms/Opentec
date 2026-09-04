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

typedef enum {
    RESPONSE_FAULT_NONE,
    RESPONSE_FAULT_LRC,
    RESPONSE_FAULT_METADATA,
} ResponseFault;

static BusCall bus_call;
static PlatformAuxBusStatus bus_status;
static uint16_t bus_register;
static const uint8_t *bus_write_data;
static uint8_t *bus_read_data;
static uint16_t bus_length;
static uint8_t command_writes;
static uint8_t response_reads;
static uint16_t ready_reads;
static uint8_t ready_rejections;

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
    ready_reads = 0;
    ready_rejections = 0;
}

static void complete_ready_read(uint8_t status) {
    assert(bus_call == BUS_CALL_READ);
    assert(bus_register == 0x07);
    assert(bus_length == 2);
    ++ready_reads;
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
    assert(((service->exchange.frame.selector >> 4) & 0x07u) == (step.phase & 0x07u));
    assert((service->exchange.frame.selector & 0x0f) == (service->sequence.use_lrc ? 4 : 0));
    if (service->sequence.stage == A71CH_AUTHENTICATION_WRITING) {
        assert(step.command == (service->sequence.use_lrc ? A71CH_AUTHENTICATION_WRITE_LRC
                                                           : A71CH_AUTHENTICATION_WRITE));
    } else if (service->sequence.stage == A71CH_AUTHENTICATION_READING) {
        assert(step.command == (service->sequence.use_lrc ? A71CH_AUTHENTICATION_READ_LRC
                                                           : A71CH_AUTHENTICATION_READ));
    } else if (service->sequence.stage == A71CH_AUTHENTICATION_FINISHING) {
        assert(step.command == (service->sequence.use_lrc ? A71CH_AUTHENTICATION_FINALIZE_LRC
                                                           : A71CH_AUTHENTICATION_FINALIZE));
        if (service->finish_recovery_pending) {
            assert(step.phase == 4);
            assert(service->exchange.frame.selector == (service->sequence.use_lrc ? 0x44 : 0x40));
        }
    }
    if (service->sequence.stage == A71CH_AUTHENTICATION_WRITING) {
        assert(memcmp(bus_write_data + 6, service->request + step.buffer_offset,
                      step.chunk_length) == 0);
    }
    ++command_writes;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void complete_response_read(A71chAuthenticationService *service, ResponseFault fault) {
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
            if (fault == RESPONSE_FAULT_LRC) {
                bus_read_data[frame->response_integrity_offset] ^= 1;
            }
        }
        if (fault == RESPONSE_FAULT_METADATA) {
            bus_read_data[bus_length - 2] = 1;
        }
    }
    ++response_reads;
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void drive_transfer_with_faults(A71chAuthenticationService *service,
                                       ResponseFault read_fault, uint8_t read_fault_budget) {
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
            complete_ready_read(ready_rejections == 0 ? 0x07 : 0x01);
            if (ready_rejections != 0) {
                --ready_rejections;
            }
        } else {
            ResponseFault fault = read_fault_budget != 0 &&
                                          service->sequence.stage == A71CH_AUTHENTICATION_READING
                                      ? read_fault
                                      : RESPONSE_FAULT_NONE;
            complete_response_read(service, fault);
            if (fault != RESPONSE_FAULT_NONE) {
                --read_fault_budget;
            }
        }
    }
}

static void drive_transfer(A71chAuthenticationService *service, ResponseFault first_read_fault) {
    drive_transfer_with_faults(service, first_read_fault,
                               first_read_fault == RESPONSE_FAULT_NONE ? 0 : 1);
}

static void drive_persistent_response_fault(A71chAuthenticationService *service,
                                            ResponseFault fault) {
    uint16_t fault_count = 0;
    bool observed_restart = false;
    for (uint16_t iteration = 0;
         iteration < 256 &&
         a71ch_authentication_service_status(service) == A71CH_AUTHENTICATION_SERVICE_RUNNING;
         ++iteration) {
        a71ch_authentication_service_run(service);
        if (fault_count >= 4 && service->sequence.stage == A71CH_AUTHENTICATION_WRITING &&
            service->sequence.phase == 0 && service->sequence.chunk_index == 0 &&
            !service->finish_recovery_pending && bus_status == PLATFORM_AUX_BUS_BUSY) {
            observed_restart = true;
        }
        if (bus_status != PLATFORM_AUX_BUS_BUSY) {
            continue;
        }
        if (bus_call == BUS_CALL_WRITE) {
            complete_command_write(service);
        } else if (bus_register == 0x07) {
            complete_ready_read(0x07);
        } else {
            bool inject_fault = service->sequence.stage == A71CH_AUTHENTICATION_READING;
            complete_response_read(service, inject_fault ? fault : RESPONSE_FAULT_NONE);
            if (inject_fault) {
                ++fault_count;
            }
        }
    }
    assert(fault_count >= 4);
    assert(observed_restart);
}

static void drive_persistent_ready_rejection(A71chAuthenticationService *service) {
    uint16_t status_polls = 0;
    for (uint16_t iteration = 0;
         iteration < 256 &&
         a71ch_authentication_service_status(service) == A71CH_AUTHENTICATION_SERVICE_RUNNING;
         ++iteration) {
        a71ch_authentication_service_run(service);
        if (bus_status != PLATFORM_AUX_BUS_BUSY) {
            continue;
        }
        if (bus_call == BUS_CALL_WRITE) {
            complete_command_write(service);
        } else {
            assert(bus_register == 0x07);
            complete_ready_read(0x01);
            ++status_polls;
        }
    }
    assert(status_polls >= 16);
}

static void drive_persistent_bus_failure(A71chAuthenticationService *service) {
    uint16_t failed_transfers = 0;
    for (uint16_t iteration = 0;
         iteration < 256 &&
         a71ch_authentication_service_status(service) == A71CH_AUTHENTICATION_SERVICE_RUNNING;
         ++iteration) {
        a71ch_authentication_service_run(service);
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            bus_status = PLATFORM_AUX_BUS_FAILED;
            ++failed_transfers;
        }
    }
    assert(failed_transfers >= 16);
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
    drive_transfer(&service, RESPONSE_FAULT_NONE);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 22);
    assert(response_reads == 22);
    assert(ready_reads == 44);
    const uint8_t *response = a71ch_authentication_service_response(&service);
    assert(response == service.response);
    for (uint16_t index = 0; index < A71CH_AUTHENTICATION_READ_SIZE; ++index) {
        assert(response[index] == (uint8_t)index);
    }
}

static void test_completes_checked_transfer(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), true));
    drive_transfer(&service, RESPONSE_FAULT_NONE);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 22);
    assert(response_reads == 22);
    assert(ready_reads == 44);
}

static void test_retries_checked_response_failure(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), true));
    drive_transfer(&service, RESPONSE_FAULT_LRC);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 27);
    assert(response_reads == 27);
    assert(ready_reads == 53);
    assert(a71ch_authentication_service_response(&service) == service.response);
}

static void test_routes_malformed_response_to_finish_recovery(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), true));
    drive_transfer(&service, RESPONSE_FAULT_METADATA);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 28);
    assert(response_reads == 28);
    assert(ready_reads == 55);
}

static void test_finishes_and_restarts_plain_malformed_response(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), false));
    drive_transfer(&service, RESPONSE_FAULT_METADATA);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 28);
    assert(response_reads == 28);
    assert(ready_reads == 55);
}

static void test_restarts_after_repeated_exchange_failures(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), true));
    drive_transfer_with_faults(&service, RESPONSE_FAULT_LRC, 4);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 42);
    assert(response_reads == 42);
    assert(ready_reads == 80);
    assert(a71ch_authentication_service_response(&service) == service.response);
}

static void test_restarts_after_rejected_ready_statuses(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    ready_rejections = 4;
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), false));
    drive_transfer(&service, RESPONSE_FAULT_NONE);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_COMPLETE);
    assert(command_writes == 22);
    assert(response_reads == 22);
    assert(ready_reads == 47);
}

static void test_persistent_protocol_failure_stays_in_recovery(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), false));
    drive_persistent_response_fault(&service, RESPONSE_FAULT_METADATA);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_RUNNING);
    assert(service.sequence.stage != A71CH_AUTHENTICATION_COMPLETE);
    assert(a71ch_authentication_service_response(&service) == 0);
}

static void test_persistent_device_failure_stays_in_recovery(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), false));
    drive_persistent_ready_rejection(&service);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_RUNNING);
    assert(service.sequence.stage == A71CH_AUTHENTICATION_WRITING);
    assert(service.sequence.phase == 0);
    assert(service.sequence.chunk_index == 0);
    assert(a71ch_authentication_service_response(&service) == 0);
}

static void test_persistent_bus_failure_stays_in_recovery(void) {
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE] = {0};
    reset_bus();
    A71chAuthenticationService service;
    a71ch_authentication_service_init(&service);
    assert(a71ch_authentication_service_start(&service, request, sizeof(request), false));
    drive_persistent_bus_failure(&service);

    assert(a71ch_authentication_service_status(&service) == A71CH_AUTHENTICATION_SERVICE_RUNNING);
    assert(service.sequence.stage == A71CH_AUTHENTICATION_WRITING);
    assert(service.sequence.phase == 0);
    assert(service.sequence.chunk_index == 0);
    assert(a71ch_authentication_service_response(&service) == 0);
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
    test_retries_checked_response_failure();
    test_routes_malformed_response_to_finish_recovery();
    test_finishes_and_restarts_plain_malformed_response();
    test_restarts_after_repeated_exchange_failures();
    test_restarts_after_rejected_ready_statuses();
    test_persistent_protocol_failure_stays_in_recovery();
    test_persistent_device_failure_stays_in_recovery();
    test_persistent_bus_failure_stays_in_recovery();
    test_rejects_invalid_or_overlapping_start();
    return 0;
}
