#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "i2c/probe.h"
#include "i2c/probe_startup_service.h"
#include "platform/aux_bus.h"

static PlatformAuxBusStatus bus_status;
static I2cProbeCommand started_command;
static uint8_t *started_response;
static uint8_t start_count;
static uint8_t clear_count;
static bool bus_accepts;

bool i2c_probe_bus_start(I2cProbeCommand command, uint8_t *response) {
    started_command = command;
    started_response = response;
    ++start_count;
    return bus_accepts;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    ++clear_count;
}

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    started_command = 0;
    started_response = 0;
    start_count = 0;
    clear_count = 0;
    bus_accepts = true;
}

static void finish_transfer(I2cProbeStartupService *service, PlatformAuxBusStatus result,
                            uint32_t now_ms) {
    bus_status = result;
    i2c_probe_startup_service_run(service, now_ms);
}

static void test_requires_explicit_start_and_idle_bus(void) {
    I2cProbeStartupService service;

    reset_bus();
    i2c_probe_startup_service_init(&service);
    i2c_probe_startup_service_run(&service, 0);
    assert(start_count == 0);
    assert(i2c_probe_startup_service_status(&service) == I2C_PROBE_STARTUP_SERVICE_IDLE);

    i2c_probe_startup_service_start(&service);
    bus_status = PLATFORM_AUX_BUS_BUSY;
    i2c_probe_startup_service_run(&service, 0);
    assert(start_count == 0);
    bus_status = PLATFORM_AUX_BUS_IDLE;
    i2c_probe_startup_service_run(&service, 0);
    assert(start_count == 1);
    assert(started_command == I2C_PROBE_BEGIN_SESSION);
    assert(started_response == 0);
}

static void test_retries_failed_and_rejected_transactions(void) {
    I2cProbeStartupService service;

    reset_bus();
    i2c_probe_startup_service_init(&service);
    i2c_probe_startup_service_start(&service);
    i2c_probe_startup_service_run(&service, 0);
    finish_transfer(&service, PLATFORM_AUX_BUS_FAILED, 1);
    assert(clear_count == 1);
    i2c_probe_startup_service_run(&service, 2);
    assert(start_count == 2);
    assert(started_command == I2C_PROBE_BEGIN_SESSION);

    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 3);
    bus_accepts = false;
    i2c_probe_startup_service_run(&service, 4);
    assert(started_command == I2C_PROBE_READ_STARTUP_STATUS);
    assert(start_count == 3);
    i2c_probe_startup_service_run(&service, 5);
    assert(start_count == 4);
    assert(started_command == I2C_PROBE_READ_STARTUP_STATUS);
}

static void test_honors_startup_status_retry_delay(void) {
    I2cProbeStartupService service;

    reset_bus();
    i2c_probe_startup_service_init(&service);
    i2c_probe_startup_service_start(&service);
    i2c_probe_startup_service_run(&service, 100);
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 100);
    i2c_probe_startup_service_run(&service, 100);
    started_response[0] = 1;
    started_response[1] = 7;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 100);

    i2c_probe_startup_service_run(&service, 105);
    assert(start_count == 2);
    i2c_probe_startup_service_run(&service, 106);
    assert(start_count == 3);
    assert(started_command == I2C_PROBE_READ_STARTUP_STATUS);
}

static void test_completes_startup_from_bus_responses(void) {
    static const uint8_t signature[] = {
        0xb8, 0x04, 0x11, 0x01, 0x05, 0x04, 0xb9, 0x02, 0x01, 0x01, 0xba, 0x01, 0x01, 0xbb, 0x0c,
        0x41, 0x37, 0x31, 0x30, 0x35, 0x43, 0x43, 0x32, 0x34, 0x32, 0x52, 0x31, 0xbc, 0x00,
    };
    I2cProbeStartupService service;

    reset_bus();
    i2c_probe_startup_service_init(&service);
    i2c_probe_startup_service_start(&service);

    i2c_probe_startup_service_run(&service, 0);
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    i2c_probe_startup_service_run(&service, 0);
    assert(started_command == I2C_PROBE_READ_STARTUP_STATUS);
    started_response[0] = 1;
    started_response[1] = 0;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    i2c_probe_startup_service_run(&service, 0);
    assert(started_command == I2C_PROBE_READ_SIGNATURE);
    started_response[0] = sizeof(signature) + 1;
    started_response[1] = 0;
    memcpy(&started_response[2], signature, sizeof(signature));
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    i2c_probe_startup_service_run(&service, 0);
    assert(started_command == I2C_PROBE_READ_CONFIRMATION);
    started_response[0] = 1;
    started_response[1] = 0xcc;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    i2c_probe_startup_service_run(&service, 0);
    assert(started_command == I2C_PROBE_READ_READY_STATUS);
    started_response[0] = 1;
    started_response[1] = 7;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    assert(i2c_probe_startup_service_status(&service) == I2C_PROBE_STARTUP_SERVICE_COMPLETE);
    i2c_probe_startup_service_run(&service, 1);
    assert(start_count == 5);
    assert(clear_count == 5);
}

int main(void) {
    test_requires_explicit_start_and_idle_bus();
    test_retries_failed_and_rejected_transactions();
    test_honors_startup_status_retry_delay();
    test_completes_startup_from_bus_responses();
    return 0;
}
