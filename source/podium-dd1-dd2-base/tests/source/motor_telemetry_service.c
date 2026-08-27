#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/telemetry_service.h"
#include "platform/aux_bus.h"

static PlatformAuxBusStatus bus_status;
static uint16_t requested_register;
static uint8_t *requested_data;
static uint16_t requested_length;
static uint8_t start_count;

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    assert(address == 0x78);
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }

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

static MotorIdentity extended_identity(void) {
    return (MotorIdentity){
        .protocol = MOTOR_PROTOCOL_POSITION_A,
    };
}

static void test_reads_extended_telemetry(void) {
    MotorTelemetryService service;
    MotorIdentity identity = extended_identity();
    reset_bus();
    motor_telemetry_service_init(&service, &identity);

    motor_telemetry_service_run(&service, 0);
    assert(requested_register == 0x12);
    assert(requested_length == 2);
    const uint8_t motor_temperature[2] = {0x34, 0x12};
    finish_read(motor_temperature);
    motor_telemetry_service_run(&service, 1);

    assert(requested_register == 0x13);
    const uint8_t driver_temperature[2] = {0x78, 0x56};
    finish_read(driver_temperature);
    motor_telemetry_service_run(&service, 2);

    assert(requested_register == 0x11);
    assert(requested_length == 4);
    const uint8_t runtime[4] = {1, 2, 3, 4};
    finish_read(runtime);
    motor_telemetry_service_run(&service, 3);

    assert(requested_register == 7);
    assert(requested_length == 1);
    const uint8_t accessory_type[1] = {9};
    finish_read(accessory_type);
    motor_telemetry_service_run(&service, 4);

    const MotorTelemetry *telemetry = motor_telemetry_service_value(&service);
    assert(telemetry->motor_temperature == 0x1234);
    assert(telemetry->driver_temperature == 0x5678);
    assert(telemetry->runtime_seconds == UINT32_C(0x04030201));
    assert(telemetry->accessory_type == 9);
    assert(telemetry->motor_temperature_valid);
    assert(telemetry->driver_temperature_valid);
    assert(telemetry->runtime_valid);
    assert(telemetry->accessory_type_valid);
    assert(start_count == 4);

    motor_telemetry_service_run(&service, 1003);
    assert(start_count == 4);
    motor_telemetry_service_run(&service, 1004);
    assert(start_count == 5);
    assert(requested_register == 0x12);
}

static void test_standard_protocol_skips_extended_values(void) {
    MotorTelemetryService service;
    const MotorIdentity identity = {
        .protocol = MOTOR_PROTOCOL_STANDARD,
    };
    reset_bus();
    motor_telemetry_service_init(&service, &identity);

    motor_telemetry_service_run(&service, 0);
    const uint8_t unavailable[2] = {0xff, 0xff};
    finish_read(unavailable);
    motor_telemetry_service_run(&service, 1);
    finish_read(unavailable);
    motor_telemetry_service_run(&service, 2);

    const MotorTelemetry *telemetry = motor_telemetry_service_value(&service);
    assert(!telemetry->motor_temperature_valid);
    assert(!telemetry->driver_temperature_valid);
    assert(!telemetry->runtime_valid);
    assert(!telemetry->accessory_type_valid);
    assert(start_count == 2);
}

static void test_failed_read_does_not_publish_value(void) {
    MotorTelemetryService service;
    MotorIdentity identity = extended_identity();
    reset_bus();
    motor_telemetry_service_init(&service, &identity);

    motor_telemetry_service_run(&service, 0);
    bus_status = PLATFORM_AUX_BUS_FAILED;
    motor_telemetry_service_run(&service, 1);

    assert(!service.telemetry.motor_temperature_valid);
    assert(requested_register == 0x13);
}

int main(void) {
    test_reads_extended_telemetry();
    test_standard_protocol_skips_extended_values();
    test_failed_read_does_not_publish_value();
    return 0;
}
