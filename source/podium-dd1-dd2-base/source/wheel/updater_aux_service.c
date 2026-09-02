#include "wheel/updater_aux_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "wheel/updater_bridge.h"

/** @brief Auxiliary-bus addresses and registers used by updater operations. */
enum {
    WHEEL_UPDATER_HANDSHAKE_ADDRESS = 0x78, /**< Auxiliary shutdown-handshake device address. */
    WHEEL_UPDATER_HANDSHAKE_REGISTER = 3,   /**< Auxiliary shutdown-handshake register. */
    WHEEL_UPDATER_AUX_ADDRESS = 0x10,       /**< Auxiliary updater device address. */
    WHEEL_UPDATER_AUX_REGISTER = 0,         /**< Auxiliary updater register offset. */
};

/** @brief Two-byte auxiliary shutdown-handshake token. */
static const uint8_t handshake[] = {0xfa, 0x05};

/**
 * @brief Makes the shared auxiliary bus available to the updater service.
 *
 * Waits for an active transaction and consumes a terminal result left by the preceding runtime
 * owner before the updater starts its next operation.
 *
 * @return True when a new transaction can start; otherwise false.
 */
static bool bus_available(void) {
    PlatformAuxBusStatus status = platform_aux_bus_status();
    if (status == PLATFORM_AUX_BUS_BUSY) {
        return false;
    }
    if (status != PLATFORM_AUX_BUS_IDLE) {
        platform_aux_bus_clear();
    }
    return true;
}

/**
 * @brief Starts one updater protocol operation on the auxiliary endpoint.
 *
 * Sends writes and reads to device address 0x10 at offset zero and retains the operation kind and
 * requested read length until the bus reports completion.
 *
 * @param[in,out] service Auxiliary updater service accepting the operation.
 * @param[in] operation Write payload or requested read length.
 */
static void start_operation(WheelUpdaterAuxService *service, WheelUpdaterOperation operation) {
    if (operation.kind == WHEEL_UPDATER_OPERATION_NONE || !bus_available()) {
        return;
    }

    bool started =
        operation.kind == WHEEL_UPDATER_OPERATION_WRITE
            ? platform_aux_bus_start_write(WHEEL_UPDATER_AUX_ADDRESS, WHEEL_UPDATER_AUX_REGISTER,
                                           operation.data, operation.length)
            : platform_aux_bus_start_read(WHEEL_UPDATER_AUX_ADDRESS, WHEEL_UPDATER_AUX_REGISTER,
                                          service->read_buffer, operation.length);
    if (started) {
        service->pending_operation = operation.kind;
        service->pending_length = operation.length;
        service->transfer_active = true;
    }
}

/**
 * @brief Initializes auxiliary updater transport state.
 *
 * Clears handshake, bus-operation, and updater response state.
 *
 * @param[out] service Auxiliary updater service to initialize.
 */
void wheel_updater_aux_service_init(WheelUpdaterAuxService *service) {
    if (service == NULL) {
        return;
    }
    *service = (WheelUpdaterAuxService){0};
    wheel_updater_bridge_init(&service->bridge);
}

/**
 * @brief Prepares auxiliary updater access after startup discovery fails.
 *
 * Treats the normal-controller shutdown prerequisite as complete because the discovery window
 * found no active normal endpoint and the recovery updater is addressed separately.
 *
 * @param[in,out] service Idle auxiliary updater service entering startup recovery.
 */
void wheel_updater_aux_service_prepare_startup_recovery(WheelUpdaterAuxService *service) {
    if (service != NULL && !service->transfer_active &&
        !wheel_updater_bridge_active(&service->bridge)) {
        service->handshake_requested = false;
        service->handshake_complete = true;
    }
}

/**
 * @brief Requests the auxiliary shutdown handshake.
 *
 * Schedules the two-byte 0x05FA token for parameter three and keeps the request pending until the
 * auxiliary bus accepts it successfully.
 *
 * @param[in,out] service Auxiliary updater service receiving the request.
 */
void wheel_updater_aux_service_request_handshake(WheelUpdaterAuxService *service) {
    if (service != NULL && !service->handshake_complete) {
        service->handshake_requested = true;
    }
}

/**
 * @brief Reports whether the auxiliary shutdown handshake completed.
 *
 * Exposes the completion latch without consuming it.
 *
 * @param[in] service Auxiliary updater service to inspect.
 * @return True after the handshake write succeeds; otherwise false.
 */
bool wheel_updater_aux_service_handshake_complete(const WheelUpdaterAuxService *service) {
    return service != NULL && service->handshake_complete;
}

/**
 * @brief Starts one updater exchange on the auxiliary endpoint.
 *
 * Requires the shutdown handshake and no outstanding bus transfer, then delegates request
 * validation and retention to the shared updater protocol.
 *
 * @param[in,out] service Idle auxiliary updater service accepting the request.
 * @param[in] request Marker-prefixed updater request.
 * @param[in] length Request byte count.
 * @param[in] response_probe True when probe-only terminal response rules apply.
 * @return True when the request was accepted; otherwise false.
 */
static bool start_exchange(WheelUpdaterAuxService *service, const uint8_t *request, uint8_t length,
                           bool response_probe) {
    if (service == NULL || !service->handshake_complete || service->transfer_active) {
        return false;
    }
    return response_probe ? wheel_updater_bridge_start_probe(&service->bridge, request, length)
                          : wheel_updater_bridge_start(&service->bridge, request, length);
}

bool wheel_updater_aux_service_start(WheelUpdaterAuxService *service, const uint8_t *request,
                                     uint8_t length) {
    return start_exchange(service, request, length, false);
}

bool wheel_updater_aux_service_start_probe(WheelUpdaterAuxService *service, const uint8_t *request,
                                           uint8_t length) {
    return start_exchange(service, request, length, true);
}

/**
 * @brief Advances the auxiliary handshake or updater exchange.
 *
 * Retries the parameter-three handshake after failed transfers. Once the handshake succeeds, it
 * converts offset-zero auxiliary-bus completions into input for the transport-independent updater
 * protocol and starts the next requested operation. A timed-out response does not cancel a
 * still-pending bus transfer.
 *
 * @param[in,out] service Auxiliary updater service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_updater_aux_service_run(WheelUpdaterAuxService *service, uint32_t now_ms) {
    if (service == NULL || (!wheel_updater_bridge_active(&service->bridge) &&
                            !service->transfer_active && !service->handshake_requested)) {
        return;
    }

    WheelUpdaterIo io = {.now_ms = now_ms};
    if (service->transfer_active) {
        PlatformAuxBusStatus status = platform_aux_bus_status();
        if (status == PLATFORM_AUX_BUS_BUSY) {
            io.status = WHEEL_UPDATER_IO_PENDING;
        } else {
            platform_aux_bus_clear();
            service->transfer_active = false;
            if (service->pending_operation == WHEEL_UPDATER_OPERATION_NONE) {
                if (status == PLATFORM_AUX_BUS_SUCCEEDED) {
                    service->handshake_requested = false;
                    service->handshake_complete = true;
                }
                return;
            }

            io.status = status == PLATFORM_AUX_BUS_SUCCEEDED ? WHEEL_UPDATER_IO_COMPLETE
                                                             : WHEEL_UPDATER_IO_FAILED;
            if (io.status == WHEEL_UPDATER_IO_COMPLETE &&
                service->pending_operation == WHEEL_UPDATER_OPERATION_READ) {
                io.data = service->read_buffer;
                io.length = service->pending_length;
            }
            service->pending_operation = WHEEL_UPDATER_OPERATION_NONE;
            service->pending_length = 0;
        }
    }

    if (service->handshake_requested) {
        if (bus_available() && platform_aux_bus_start_write(WHEEL_UPDATER_HANDSHAKE_ADDRESS,
                                                            WHEEL_UPDATER_HANDSHAKE_REGISTER,
                                                            handshake, sizeof(handshake))) {
            service->transfer_active = true;
        }
        return;
    }

    WheelUpdaterOperation operation = wheel_updater_bridge_step(&service->bridge, io);
    if (!service->transfer_active) {
        start_operation(service, operation);
    }
}

/**
 * @brief Takes one complete auxiliary updater response.
 *
 * Delegates retained-response ownership to the transport-independent updater protocol.
 *
 * @param[in,out] service Auxiliary updater service holding a response.
 * @param[out] response Complete response bytes.
 * @param[out] length Complete response length.
 * @return True when a response was available; otherwise false.
 */
bool wheel_updater_aux_service_take_response(WheelUpdaterAuxService *service,
                                             const uint8_t **response, uint8_t *length) {
    return service != NULL &&
           wheel_updater_bridge_take_response(&service->bridge, response, length);
}

/**
 * @brief Reports whether the auxiliary updater service owns work.
 *
 * Includes a requested handshake, an active bus transaction, and every updater protocol phase.
 *
 * @param[in] service Auxiliary updater service to inspect.
 * @return True while the service owns pending work; otherwise false.
 */
bool wheel_updater_aux_service_active(const WheelUpdaterAuxService *service) {
    return service != NULL && (service->handshake_requested || service->transfer_active ||
                               wheel_updater_bridge_active(&service->bridge));
}
