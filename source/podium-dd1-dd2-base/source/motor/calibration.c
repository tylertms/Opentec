#include "motor/calibration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/aux_bus.h"

enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78,
    MOTOR_CALIBRATION_REGISTER = 6,
    MOTOR_CALIBRATION_COMMAND_SIZE = 7,
    MOTOR_CALIBRATION_COMMAND_PREFIX = 0xf9,
    MOTOR_CALIBRATION_COMMAND_SUBCOMMAND = 4,
    MOTOR_CALIBRATION_CALIBRATE_REQUEST = 1u << 0,
    MOTOR_CALIBRATION_ERASE_REQUEST = 1u << 1,
    MOTOR_CALIBRATION_CALIBRATE_VALUE = 0xaa,
    MOTOR_CALIBRATION_ERASE_VALUE = 0xbb,
    MOTOR_CALIBRATION_REQUIRED_WHEEL_MODE = 0,
};

/**
 * @brief Decodes a host motor-calibration request.
 *
 * Accepts seven-byte short reports beginning with F9 04 when all five remaining bytes are AA or
 * all five are BB. The repeated AA form requests calibration and the repeated BB form requests
 * erasure.
 *
 * @param[in] output Classified USB output report.
 * @param[out] operation Requested motor-calibration operation.
 * @return True when the complete report matches either calibration signature.
 */
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

/**
 * @brief Initializes asynchronous motor-calibration state.
 *
 * Clears pending operation requests, transfer ownership, response data, lifecycle event, and the
 * active phase.
 *
 * @param[out] service Motor-calibration service to initialize.
 */
void motor_calibration_service_init(MotorCalibrationService *service) {
    *service = (MotorCalibrationService){0};
}

/**
 * @brief Queues a motor-calibration operation.
 *
 * Retains calibration and erasure independently so calibration keeps priority when both are
 * pending.
 *
 * @param[in,out] service Motor-calibration service and pending requests.
 * @param[in] operation Calibration or erasure to queue.
 */
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

    uint8_t value = service->operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE
                        ? MOTOR_CALIBRATION_CALIBRATE_VALUE
                        : MOTOR_CALIBRATION_ERASE_VALUE;
    service->data[0] = value;
    service->data[1] = value;
    service->phase = MOTOR_CALIBRATION_WRITE_COMMAND;
    if (service->operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE) {
        service->event = MOTOR_CALIBRATION_EVENT_STARTED;
    }
    return true;
}

/**
 * @brief Applies one completed motor-calibration bus transfer.
 *
 * Advances a successful command write to response polling and completes the selected request with
 * its result event when a successful response read is nonzero. Failed transfers retain their
 * current phase for retry.
 *
 * @param[in,out] service Motor-calibration phase, data, and transfer state.
 * @param[in] succeeded True when the auxiliary-bus transfer completed successfully.
 */
static void finish_transfer(MotorCalibrationService *service, bool succeeded) {
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }
    if (service->phase == MOTOR_CALIBRATION_WRITE_COMMAND) {
        service->data[0] = 0;
        service->data[1] = 0;
        service->phase = MOTOR_CALIBRATION_READ_RESPONSE;
    } else if (service->data[0] != 0 || service->data[1] != 0) {
        service->requests &= (uint8_t)~request_bit(service->operation);
        service->phase = MOTOR_CALIBRATION_IDLE;
        service->event = service->operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE
                             ? MOTOR_CALIBRATION_EVENT_COMPLETED
                             : MOTOR_CALIBRATION_EVENT_ERASED;
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

/**
 * @brief Advances the asynchronous motor-calibration exchange.
 *
 * Validates the selected operation against wheel mode and accessory availability, writes AA AA or
 * BB BB to motor register 6, and polls the same two-byte register until the controller returns a
 * nonzero response. Calibration has priority when both operations are queued.
 *
 * @param[in,out] service Motor-calibration requests, phase, and transfer state.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] telemetry Current motor-controller telemetry and accessory availability.
 */
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

/**
 * @brief Reports whether motor calibration has queued or active work.
 *
 * Includes retained operation requests, a controller exchange phase, and an in-flight bus
 * transfer.
 *
 * @param[in] service Motor-calibration service state.
 * @return True while calibration work remains outstanding.
 */
bool motor_calibration_service_pending(const MotorCalibrationService *service) {
    return service->requests != 0 || service->phase != MOTOR_CALIBRATION_IDLE ||
           service->transfer_active;
}

/**
 * @brief Reports whether motor calibration owns the auxiliary bus.
 *
 * Distinguishes an active calibration transfer from a queued request that must wait for the current
 * background motor service to release the shared bus.
 *
 * @param[in] service Motor-calibration service state.
 * @return True while a calibration read or write is in flight.
 */
bool motor_calibration_service_owns_bus(const MotorCalibrationService *service) {
    return service->transfer_active;
}

/**
 * @brief Returns the latest motor-calibration lifecycle event.
 *
 * Exposes disconnect-wheel and unsupported rejections, calibration start and completion, and
 * successful calibration-data erasure.
 *
 * @param[in] service Motor-calibration service state.
 * @return Most recent calibration lifecycle event.
 */
MotorCalibrationEvent motor_calibration_service_event(const MotorCalibrationService *service) {
    return service->event;
}
