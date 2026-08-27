#include "motor/telemetry_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "platform/time.h"

enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78,
    MOTOR_ACCESSORY_TYPE_REGISTER = 0x07,
    MOTOR_RUNTIME_REGISTER = 0x11,
    MOTOR_TEMPERATURE_REGISTER = 0x12,
    MOTOR_DRIVER_TEMPERATURE_REGISTER = 0x13,
    MOTOR_TELEMETRY_POLL_INTERVAL_MS = 1000,
};

void motor_telemetry_service_init(MotorTelemetryService *service, const MotorIdentity *identity) {
    motor_telemetry_init(&service->telemetry);
    service->read = MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE;
    service->next_poll_ms = 0;
    service->extended = motor_identity_has_extended_parameters(identity);
    service->transfer_active = false;
}

static void store_read(MotorTelemetryService *service) {
    switch (service->read) {
    case MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE:
        motor_telemetry_set_motor_temperature(&service->telemetry, service->data);
        break;
    case MOTOR_TELEMETRY_READ_DRIVER_TEMPERATURE:
        motor_telemetry_set_driver_temperature(&service->telemetry, service->data);
        break;
    case MOTOR_TELEMETRY_READ_RUNTIME:
        motor_telemetry_set_runtime(&service->telemetry, service->data);
        break;
    case MOTOR_TELEMETRY_READ_ACCESSORY_TYPE:
        motor_telemetry_set_accessory_type(&service->telemetry, service->data[0]);
        break;
    }
}

static void advance_read(MotorTelemetryService *service, uint32_t now_ms) {
    if (service->read == MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE) {
        service->read = MOTOR_TELEMETRY_READ_DRIVER_TEMPERATURE;
    } else if (service->read == MOTOR_TELEMETRY_READ_DRIVER_TEMPERATURE && service->extended) {
        service->read = MOTOR_TELEMETRY_READ_RUNTIME;
    } else if (service->read == MOTOR_TELEMETRY_READ_RUNTIME) {
        service->read = MOTOR_TELEMETRY_READ_ACCESSORY_TYPE;
    } else {
        service->read = MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE;
        service->next_poll_ms = now_ms + MOTOR_TELEMETRY_POLL_INTERVAL_MS;
    }
}

static void start_read(MotorTelemetryService *service) {
    uint16_t address;
    uint16_t length;

    switch (service->read) {
    case MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE:
        address = MOTOR_TEMPERATURE_REGISTER;
        length = 2;
        break;
    case MOTOR_TELEMETRY_READ_DRIVER_TEMPERATURE:
        address = MOTOR_DRIVER_TEMPERATURE_REGISTER;
        length = 2;
        break;
    case MOTOR_TELEMETRY_READ_RUNTIME:
        address = MOTOR_RUNTIME_REGISTER;
        length = 4;
        break;
    case MOTOR_TELEMETRY_READ_ACCESSORY_TYPE:
        address = MOTOR_ACCESSORY_TYPE_REGISTER;
        length = 1;
        break;
    default:
        return;
    }

    service->transfer_active =
        platform_aux_bus_start_read(MOTOR_AUX_BUS_ADDRESS, address, service->data, length);
}

void motor_telemetry_service_run(MotorTelemetryService *service, uint32_t now_ms) {
    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (service->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        if (bus_status == PLATFORM_AUX_BUS_SUCCEEDED) {
            store_read(service);
        }
        platform_aux_bus_clear();
        service->transfer_active = false;
        advance_read(service, now_ms);
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }

    if (bus_status == PLATFORM_AUX_BUS_IDLE &&
        platform_time_reached(now_ms, service->next_poll_ms)) {
        start_read(service);
    }
}

const MotorTelemetry *motor_telemetry_service_value(const MotorTelemetryService *service) {
    return &service->telemetry;
}
