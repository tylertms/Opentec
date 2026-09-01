#include "motor/calibration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/aux_bus.h"

/** @brief Internal constants for the motor calibration bus protocol. */
enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78, /**< Auxiliary-bus address of the motor controller. */
    MOTOR_CALIBRATION_REGISTER =
        6, /**< Controller register used for calibration commands and results. */
    MOTOR_CALIBRATION_COMMAND_SIZE = 7, /**< Length of the host calibration command signature. */
    MOTOR_CALIBRATION_COMMAND_PREFIX = 0xf9,       /**< Host calibration command prefix byte. */
    MOTOR_CALIBRATION_COMMAND_SUBCOMMAND = 4,      /**< Host calibration command subcommand byte. */
    MOTOR_CALIBRATION_CALIBRATE_REQUEST = 1u << 0, /**< Pending-request bit for calibration. */
    MOTOR_CALIBRATION_ERASE_REQUEST = 1u << 1,     /**< Pending-request bit for erasure. */
    MOTOR_CALIBRATION_CALIBRATE_VALUE = 0xaa,      /**< Repeated command value for calibration. */
    MOTOR_CALIBRATION_ERASE_VALUE = 0xbb,          /**< Repeated command value for erasure. */
    MOTOR_CALIBRATION_REQUIRED_WHEEL_MODE = 0,     /**< Wheel mode required before calibration. */
};

bool motor_calibration_command_decode(const UsbOutputCommand *output,
                                      MotorCalibrationOperation *operation) {
    if (output == NULL || operation == NULL || output->kind != USB_OUTPUT_COMMAND_SHORT ||
        output->payload == NULL || output->length != MOTOR_CALIBRATION_COMMAND_SIZE ||
        output->payload[0] != MOTOR_CALIBRATION_COMMAND_PREFIX ||
        output->payload[1] != MOTOR_CALIBRATION_COMMAND_SUBCOMMAND) {
        return false;
    }

    uint8_t value = output->payload[2];
    if (value != MOTOR_CALIBRATION_CALIBRATE_VALUE && value != MOTOR_CALIBRATION_ERASE_VALUE) {
        return false;
    }
    for (uint8_t index = 3; index < MOTOR_CALIBRATION_COMMAND_SIZE; index++) {
        if (output->payload[index] != value) {
            return false;
        }
    }
    *operation = value == MOTOR_CALIBRATION_CALIBRATE_VALUE ? MOTOR_CALIBRATION_OPERATION_CALIBRATE
                                                            : MOTOR_CALIBRATION_OPERATION_ERASE;
    return true;
}

void motor_calibration_service_init(MotorCalibrationService *service) {
    *service = (MotorCalibrationService){0};
}

void motor_calibration_service_request(MotorCalibrationService *service,
                                       MotorCalibrationOperation operation) {
    service->requests |= operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE
                             ? MOTOR_CALIBRATION_CALIBRATE_REQUEST
                             : MOTOR_CALIBRATION_ERASE_REQUEST;
}

/**
 * @brief Selects the pending-bit mask for a calibration operation.
 *
 * Maps calibration and erasure to their independent retained request bits.
 *
 * @param[in] operation Calibration or erasure.
 * @return Pending-bit mask for the selected operation.
 */
static uint8_t request_bit(MotorCalibrationOperation operation) {
    return operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE ? MOTOR_CALIBRATION_CALIBRATE_REQUEST
                                                              : MOTOR_CALIBRATION_ERASE_REQUEST;
}

/**
 * @brief Starts the highest-priority eligible calibration request.
 *
 * Selects calibration before erasure, rejects calibration outside wheel mode zero, rejects either
 * operation before an accessory type is available, records the corresponding lifecycle event, and
 * prepares the repeated command bytes for an accepted request.
 *
 * @param[in,out] service Motor-calibration requests and command state.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] telemetry Current motor-controller telemetry and accessory availability.
 * @return True when an eligible request entered the command-write phase.
 */
static bool begin_request(MotorCalibrationService *service, uint8_t wheel_mode,
                          const MotorTelemetry *telemetry) {
    service->operation = (service->requests & MOTOR_CALIBRATION_CALIBRATE_REQUEST) != 0
                             ? MOTOR_CALIBRATION_OPERATION_CALIBRATE
                             : MOTOR_CALIBRATION_OPERATION_ERASE;
    if (service->operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE &&
        wheel_mode != MOTOR_CALIBRATION_REQUIRED_WHEEL_MODE) {
        service->event = MOTOR_CALIBRATION_EVENT_DISCONNECT_WHEEL;
        service->requests &= (uint8_t)~request_bit(service->operation);
        return false;
    }
    if (telemetry == NULL || !telemetry->accessory_type_valid) {
        service->event = MOTOR_CALIBRATION_EVENT_UNSUPPORTED;
        service->requests &= (uint8_t)~request_bit(service->operation);
        return false;
    }

    service->data[0] = 0;
    service->data[1] = 0;
    service->phase = MOTOR_CALIBRATION_READ_IDLE;
    return true;
}

/**
 * @brief Applies one completed motor-calibration bus transfer.
 *
 * Advances a successful command write to response polling and completes the selected request with
 * its result event when a successful response read is nonzero. A completed calibration holds its
 * result state for four seconds, while a completed erase returns directly to idle. Failed
 * transfers retain their current phase for retry.
 *
 * @param[in,out] service Motor-calibration phase, data, and transfer state.
 * @param[in] succeeded True when the auxiliary-bus transfer completed successfully.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void finish_transfer(MotorCalibrationService *service, bool succeeded) {
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }
    if (service->phase == MOTOR_CALIBRATION_READ_IDLE) {
        if (service->data[0] != 0 || service->data[1] != 0) {
            return;
        }
        uint8_t value = service->operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE
                            ? MOTOR_CALIBRATION_CALIBRATE_VALUE
                            : MOTOR_CALIBRATION_ERASE_VALUE;
        service->data[0] = value;
        service->data[1] = value;
        service->phase = MOTOR_CALIBRATION_WRITE_COMMAND;
        if (service->operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE) {
            service->event = MOTOR_CALIBRATION_EVENT_STARTED;
        }
    } else if (service->phase == MOTOR_CALIBRATION_WRITE_COMMAND) {
        service->data[0] = 0;
        service->data[1] = 0;
        service->phase = MOTOR_CALIBRATION_READ_RESPONSE;
    } else if (service->data[0] == 0 && service->data[1] == 0) {
        service->requests &= (uint8_t)~request_bit(service->operation);
        if (service->operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE) {
            service->event = MOTOR_CALIBRATION_EVENT_COMPLETED;
        } else {
            service->event = MOTOR_CALIBRATION_EVENT_ERASED;
        }
        service->phase = MOTOR_CALIBRATION_IDLE;
    }
}

/**
 * @brief Starts the transfer required by the current calibration phase.
 *
 * Writes the two-byte command during the command phase and reads the two-byte response during the
 * polling phase.
 *
 * @param[in,out] service Motor-calibration phase, data, and transfer ownership.
 */
static void start_transfer(MotorCalibrationService *service) {
    if (service->phase == MOTOR_CALIBRATION_WRITE_COMMAND) {
        service->transfer_active =
            platform_aux_bus_start_write(MOTOR_AUX_BUS_ADDRESS, MOTOR_CALIBRATION_REGISTER,
                                         service->data, sizeof(service->data));
    } else {
        service->transfer_active =
            platform_aux_bus_start_read(MOTOR_AUX_BUS_ADDRESS, MOTOR_CALIBRATION_REGISTER,
                                        service->data, sizeof(service->data));
    }
}

void motor_calibration_service_run(MotorCalibrationService *service, uint8_t wheel_mode,
                                   const MotorTelemetry *telemetry) {
    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (service->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        finish_transfer(service, bus_status == PLATFORM_AUX_BUS_SUCCEEDED);
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }

    while (service->phase == MOTOR_CALIBRATION_IDLE && service->requests != 0) {
        if (begin_request(service, wheel_mode, telemetry)) {
            break;
        }
    }
    if (bus_status == PLATFORM_AUX_BUS_IDLE && service->phase != MOTOR_CALIBRATION_IDLE) {
        start_transfer(service);
    }
}

bool motor_calibration_service_pending(const MotorCalibrationService *service) {
    return service->requests != 0 || service->phase != MOTOR_CALIBRATION_IDLE ||
           service->transfer_active;
}

/**
 * @brief Reports whether motor calibration is in an active exchange phase.
 *
 * @param[in] service Calibration service to inspect.
 * @return True while an exchange is active; otherwise false.
 */
bool motor_calibration_service_active(const MotorCalibrationService *service) {
    return service->phase == MOTOR_CALIBRATION_WRITE_COMMAND ||
           service->phase == MOTOR_CALIBRATION_READ_RESPONSE;
}

bool motor_calibration_service_owns_bus(const MotorCalibrationService *service) {
    return service->transfer_active;
}

MotorCalibrationEvent motor_calibration_service_take_event(MotorCalibrationService *service) {
    MotorCalibrationEvent event = service->event;
    service->event = MOTOR_CALIBRATION_EVENT_NONE;
    return event;
}
