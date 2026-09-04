#include "motor/telemetry_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "platform/time.h"

/**
 * @brief Auxiliary-bus registers and timing for motor telemetry acquisition.
 */
enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78,             /**< Auxiliary-bus address of the motor controller. */
    MOTOR_ACCESSORY_TYPE_REGISTER = 0x07,     /**< Register containing the accessory type. */
    MOTOR_RUNTIME_REGISTER = 0x11,            /**< Register containing motor runtime in seconds. */
    MOTOR_TEMPERATURE_REGISTER = 0x12,        /**< Register containing motor temperature. */
    MOTOR_DRIVER_TEMPERATURE_REGISTER = 0x13, /**< Register containing driver temperature. */
    MOTOR_TELEMETRY_POLL_INTERVAL_MS = 200, /**< Delay between telemetry passes in milliseconds. */
};

/**
 * @brief Initializes periodic motor telemetry acquisition.
 *
 * Clears published telemetry, selects motor temperature as the first register, and enables the
 * runtime and accessory registers only for controllers with extended parameters.
 *
 * @param[out] service Motor telemetry service to initialize.
 * @param[in] identity Identified motor-controller protocol.
 */
void motor_telemetry_service_init(MotorTelemetryService *service, const MotorIdentity *identity) {
    motor_telemetry_init(&service->telemetry);
    service->read = MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE;
    service->next_poll_ms = 0;
    service->extended = motor_identity_has_extended_parameters(identity);
    service->transfer_phase = MOTOR_TELEMETRY_TRANSFER_QUEUE;
}

/**
 * @brief Publishes the completed telemetry register value.
 *
 * Routes the current transfer buffer to its typed telemetry channel.
 *
 * @param[in,out] service Motor telemetry service containing the completed read.
 */
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

/**
 * @brief Selects the next telemetry register.
 *
 * Advances through both temperatures and, for extended controllers, runtime and accessory type.
 * Finishing the sequence schedules its next pass 200 milliseconds later.
 *
 * @param[in,out] service Motor telemetry service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
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

/**
 * @brief Starts the selected motor telemetry read.
 *
 * Maps the current channel to its register address and byte count, then requests one auxiliary-bus
 * transfer to motor address 0x78.
 *
 * @param[in,out] service Motor telemetry service to start.
 */
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

    bool started =
        platform_aux_bus_start_read(MOTOR_AUX_BUS_ADDRESS, address, service->data, length);
    service->transfer_phase = started ? MOTOR_TELEMETRY_TRANSFER_WAIT
                                      : MOTOR_TELEMETRY_TRANSFER_ERROR;
}

/**
 * @brief Advances periodic motor telemetry acquisition.
 *
 * Publishes successful reads, gives a failed transfer one error rearm pass, and starts the next due
 * transfer only after a fresh check confirms that the shared auxiliary bus is idle.
 *
 * @param[in,out] service Motor telemetry service state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_telemetry_service_run(MotorTelemetryService *service, uint32_t now_ms) {
    if (service->transfer_phase == MOTOR_TELEMETRY_TRANSFER_ERROR) {
        service->transfer_phase = MOTOR_TELEMETRY_TRANSFER_QUEUE;
        platform_aux_bus_clear();
        return;
    }

    if (service->transfer_phase == MOTOR_TELEMETRY_TRANSFER_WAIT) {
        PlatformAuxBusStatus bus_status = platform_aux_bus_status();
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        if (bus_status == PLATFORM_AUX_BUS_SUCCEEDED) {
            store_read(service);
            platform_aux_bus_clear();
            service->transfer_phase = MOTOR_TELEMETRY_TRANSFER_QUEUE;
            advance_read(service, now_ms);
        } else if (bus_status == PLATFORM_AUX_BUS_FAILED) {
            service->transfer_phase = MOTOR_TELEMETRY_TRANSFER_ERROR;
            return;
        } else {
            return;
        }
    }

    if (service->transfer_phase != MOTOR_TELEMETRY_TRANSFER_QUEUE ||
        !platform_time_reached(now_ms, service->next_poll_ms) ||
        platform_aux_bus_status() != PLATFORM_AUX_BUS_IDLE) {
        return;
    }

    start_read(service);
}

/**
 * @brief Returns the latest accepted motor telemetry.
 *
 * Provides the retained values and per-channel availability flags owned by the service.
 *
 * @param[in] service Motor telemetry service state.
 * @return Current motor telemetry snapshot.
 */
const MotorTelemetry *motor_telemetry_service_value(const MotorTelemetryService *service) {
    return &service->telemetry;
}
