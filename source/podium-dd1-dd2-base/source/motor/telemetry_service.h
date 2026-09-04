#ifndef OPENTEC_BASE_MOTOR_TELEMETRY_SERVICE_H
#define OPENTEC_BASE_MOTOR_TELEMETRY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/telemetry.h"

/**
 * @brief Telemetry channels read by the motor telemetry service.
 *
 * Identifies the register and response width selected for the current auxiliary-bus transfer.
 */
typedef enum {
    MOTOR_TELEMETRY_READ_MOTOR_TEMPERATURE,  /**< Read the motor-temperature register. */
    MOTOR_TELEMETRY_READ_DRIVER_TEMPERATURE, /**< Read the driver-temperature register. */
    MOTOR_TELEMETRY_READ_RUNTIME,            /**< Read the motor runtime register. */
    MOTOR_TELEMETRY_READ_ACCESSORY_TYPE,     /**< Read the accessory-type register. */
} MotorTelemetryRead;

/**
 * @brief Transfer phase for the selected motor telemetry read.
 *
 * A failed start or terminal transfer enters the error phase for one service pass. That pass rearms
 * the selected read without starting a replacement transfer; the following pass may queue it when
 * the shared auxiliary bus is idle.
 */
typedef enum {
    MOTOR_TELEMETRY_TRANSFER_QUEUE = 0, /**< The selected read may be queued. */
    MOTOR_TELEMETRY_TRANSFER_WAIT = 1,  /**< The selected read is active on the auxiliary bus. */
    MOTOR_TELEMETRY_TRANSFER_ERROR = 2, /**< The failed read awaits its rearm pass. */
} MotorTelemetryTransferPhase;

/**
 * @brief Periodic motor telemetry acquisition state.
 *
 * Stores the latest telemetry, selected channel, transfer buffer, poll deadline, and controller
 * capability used by the acquisition loop.
 */
typedef struct {
    MotorTelemetry telemetry; /**< Latest accepted telemetry values. */
    MotorTelemetryRead read;  /**< Channel selected for the current or next transfer. */
    uint8_t data[4];          /**< Buffer for the selected register response. */
    uint32_t next_poll_ms;    /**< Monotonic deadline for the next telemetry pass. */
    bool extended; /**< Extended runtime and accessory registers are available. */
    MotorTelemetryTransferPhase transfer_phase; /**< Current queue, wait, or error phase. */
} MotorTelemetryService;

/**
 * @brief Initializes periodic motor telemetry acquisition.
 *
 * Clears published telemetry, selects motor temperature as the first channel, and enables extended
 * channels according to the identified controller.
 *
 * @param[out] service Motor telemetry service state to initialize.
 * @param[in] identity Identified motor-controller protocol.
 */
void motor_telemetry_service_init(MotorTelemetryService *service, const MotorIdentity *identity);

/**
 * @brief Advances periodic motor telemetry acquisition.
 *
 * Completes an active read, stores successful responses, and starts the next due register read when
 * the shared auxiliary bus is idle. A failed read remains selected while one error phase rearms
 * it, so the retry starts only after that dedicated pass and a fresh idle-bus check. The failure
 * path does not advance the channel or change the periodic poll deadline.
 *
 * @param[in,out] service Motor telemetry service state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_telemetry_service_run(MotorTelemetryService *service, uint32_t now_ms);

/**
 * @brief Returns the latest accepted motor telemetry.
 *
 * Provides the service-owned values and per-channel validity flags.
 *
 * @param[in] service Motor telemetry service state.
 * @return Latest motor telemetry snapshot.
 */
const MotorTelemetry *motor_telemetry_service_value(const MotorTelemetryService *service);

#endif
