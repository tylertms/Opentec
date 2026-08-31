#ifndef OPENTEC_BASE_MOTOR_CALIBRATION_H
#define OPENTEC_BASE_MOTOR_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/telemetry.h"
#include "usb/output_command.h"

/** @brief Selects the motor calibration operation requested by the host. */
typedef enum {
    MOTOR_CALIBRATION_OPERATION_CALIBRATE, /**< Runs the wheel calibration procedure. */
    MOTOR_CALIBRATION_OPERATION_ERASE, /**< Erases the stored wheel calibration data. */
} MotorCalibrationOperation;

/** @brief Identifies the asynchronous phase of a motor calibration exchange. */
typedef enum {
    MOTOR_CALIBRATION_IDLE, /**< No operation is currently being transferred; queued requests may remain. */
    MOTOR_CALIBRATION_WRITE_COMMAND, /**< The calibration or erase command is being written. */
    MOTOR_CALIBRATION_READ_RESPONSE, /**< The controller is being polled for a nonzero result. */
    MOTOR_CALIBRATION_HOLD_RESULT, /**< A completed calibration result is being held. */
} MotorCalibrationPhase;

/** @brief Reports a motor calibration lifecycle transition. */
typedef enum {
    MOTOR_CALIBRATION_EVENT_NONE = 0, /**< No event is waiting to be consumed. */
    MOTOR_CALIBRATION_EVENT_DISCONNECT_WHEEL = 8, /**< Calibration was rejected because the wheel must be disconnected. */
    MOTOR_CALIBRATION_EVENT_UNSUPPORTED = 9, /**< The operation was rejected because the accessory type is unavailable. */
    MOTOR_CALIBRATION_EVENT_COMPLETED = 10, /**< Calibration completed successfully. */
    MOTOR_CALIBRATION_EVENT_ERASED = 11, /**< Stored calibration data was erased successfully. */
    MOTOR_CALIBRATION_EVENT_STARTED = 0x1c, /**< Calibration started after its request passed validation. */
} MotorCalibrationEvent;

/** @brief Stores state for the asynchronous motor calibration service. */
typedef struct {
    MotorCalibrationPhase phase; /**< Current asynchronous exchange phase. */
    MotorCalibrationOperation operation; /**< Operation currently selected for the exchange. */
    MotorCalibrationEvent event; /**< Latest lifecycle event awaiting publication. */
    uint32_t result_deadline_ms; /**< Monotonic deadline for releasing a completed calibration result. */
    uint8_t data[2]; /**< Command bytes written to, or response bytes read from, the controller. */
    uint8_t requests; /**< Bit mask of calibration and erase requests waiting to run. */
    bool transfer_active; /**< Whether an auxiliary-bus transfer is currently in flight. */
} MotorCalibrationService;

/**
 * @brief Decodes a host motor-calibration request.
 *
 * Recognizes the seven-byte short-report signatures used by the host and writes the corresponding
 * calibration or erase operation to the caller's output value.
 *
 * @param[in] output Classified USB output report to inspect.
 * @param[out] operation Decoded motor-calibration operation.
 * @return true when the report matches a complete calibration or erase signature; otherwise false.
 */
bool motor_calibration_command_decode(const UsbOutputCommand *output,
                                      MotorCalibrationOperation *operation);

/**
 * @brief Initializes a motor-calibration service.
 *
 * Clears queued requests, exchange state, response bytes, deadlines, and the pending lifecycle
 * event. The service pointer must refer to writable storage.
 *
 * @param[out] service Motor-calibration service state to initialize.
 */
void motor_calibration_service_init(MotorCalibrationService *service);

/**
 * @brief Queues a motor-calibration operation.
 *
 * Retains the request until the service can run it and preserves calibration priority when both
 * calibration and erasure are queued.
 *
 * @param[in,out] service Motor-calibration service receiving the request.
 * @param[in] operation Calibration or erasure operation to queue.
 */
void motor_calibration_service_request(MotorCalibrationService *service,
                                       MotorCalibrationOperation operation);

/**
 * @brief Advances the asynchronous motor-calibration exchange.
 *
 * Validates queued work, coordinates the auxiliary-bus write and response polling, and records
 * lifecycle events as requests are rejected or completed.
 *
 * @param[in,out] service Motor-calibration service state to advance.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] telemetry Motor-controller telemetry used to verify accessory availability.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_calibration_service_run(MotorCalibrationService *service, uint8_t wheel_mode,
                                   const MotorTelemetry *telemetry, uint32_t now_ms);

/**
 * @brief Tests whether motor-calibration work remains pending.
 *
 * Includes queued requests, an active exchange phase, a held completion result, and an in-flight
 * auxiliary-bus transfer.
 *
 * @param[in] service Motor-calibration service state to inspect.
 * @return true while any calibration work remains outstanding; otherwise false.
 */
bool motor_calibration_service_pending(const MotorCalibrationService *service);

/**
 * @brief Tests whether motor calibration currently owns the auxiliary bus.
 *
 * Reports only an in-flight read or write, so a queued request that is waiting for bus ownership
 * does not claim the bus.
 *
 * @param[in] service Motor-calibration service state to inspect.
 * @return true while a calibration transfer is in flight; otherwise false.
 */
bool motor_calibration_service_owns_bus(const MotorCalibrationService *service);

/**
 * @brief Takes the pending motor-calibration lifecycle event.
 *
 * Returns the current event and clears it so each rejection or completion is published once.
 *
 * @param[in,out] service Motor-calibration service whose event is consumed.
 * @return Pending lifecycle event, or MOTOR_CALIBRATION_EVENT_NONE when no event is waiting.
 */
MotorCalibrationEvent motor_calibration_service_take_event(MotorCalibrationService *service);

#endif
