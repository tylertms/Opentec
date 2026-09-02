#include "wheel/updater_direct_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/serial_link.h"
#include "wheel/updater_bridge.h"

/**
 * @brief Polls one raw updater transport operation.
 *
 * Completes accepted writes immediately for the protocol layer and waits for each complete read
 * fragment to accumulate on the direct UART link.
 *
 * @param[in,out] service Direct updater service polling its pending operation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Updater protocol input for the current service iteration.
 */
static WheelUpdaterIo poll_operation(WheelUpdaterDirectService *service, uint32_t now_ms) {
    WheelUpdaterIo io = {.now_ms = now_ms};
    if (!service->operation_pending) {
        return io;
    }
    if (service->pending_operation == WHEEL_UPDATER_OPERATION_WRITE) {
        service->operation_pending = false;
        io.status = WHEEL_UPDATER_IO_COMPLETE;
        return io;
    }
    if (!platform_serial_link_direct_read(service->read_buffer, service->pending_length)) {
        io.status = WHEEL_UPDATER_IO_PENDING;
        return io;
    }
    service->operation_pending = false;
    io.data = service->read_buffer;
    io.length = service->pending_length;
    io.status = WHEEL_UPDATER_IO_COMPLETE;
    return io;
}

/**
 * @brief Starts one raw updater transport operation.
 *
 * Queues writes on UART3 and retains sized reads for polling. Rejected writes remain available for
 * retry on the next service iteration.
 *
 * @param[in,out] service Direct updater service accepting the operation.
 * @param[in] operation Write request or sized read returned by the protocol.
 */
static void start_operation(WheelUpdaterDirectService *service, WheelUpdaterOperation operation) {
    if (operation.kind == WHEEL_UPDATER_OPERATION_NONE) {
        return;
    }
    if (operation.kind == WHEEL_UPDATER_OPERATION_WRITE &&
        !platform_serial_link_direct_write(operation.data, operation.length)) {
        return;
    }
    service->pending_operation = operation.kind;
    service->pending_length = operation.length;
    service->operation_pending = true;
}

/**
 * @brief Initializes raw updater service state.
 *
 * Clears protocol and pending-operation state for a later status-bridge exchange.
 *
 * @param[out] service Direct updater service to initialize.
 */
void wheel_updater_direct_service_init(WheelUpdaterDirectService *service) {
    if (service == NULL) {
        return;
    }
    *service = (WheelUpdaterDirectService){0};
    wheel_updater_bridge_init(&service->bridge);
}

/**
 * @brief Starts one normal or route-probe exchange on the status bridge.
 *
 * Rejects an outstanding direct-UART operation and delegates marker, length, and ownership
 * validation to the common updater protocol.
 *
 * @param[in,out] service Idle direct updater service accepting the request.
 * @param[in] request Marker-prefixed updater request.
 * @param[in] length Request byte count.
 * @param[in] response_probe True when probe-only terminal response rules apply.
 * @return true when the request was accepted; otherwise false.
 */
static bool start_exchange(WheelUpdaterDirectService *service, const uint8_t *request,
                           uint8_t length, bool response_probe) {
    if (service == NULL || service->operation_pending) {
        return false;
    }
    return response_probe ? wheel_updater_bridge_start_probe(&service->bridge, request, length)
                          : wheel_updater_bridge_start(&service->bridge, request, length);
}

bool wheel_updater_direct_service_start(WheelUpdaterDirectService *service, const uint8_t *request,
                                        uint8_t length) {
    return start_exchange(service, request, length, false);
}

bool wheel_updater_direct_service_start_probe(WheelUpdaterDirectService *service,
                                              const uint8_t *request, uint8_t length) {
    return start_exchange(service, request, length, true);
}

/**
 * @brief Advances updater protocol operations over the raw UART link.
 *
 * Polls a pending byte operation, advances the common response parser, and starts its next raw read
 * or write without exposing peripheral state to the protocol layer. A timed-out response does not
 * cancel a still-pending direct read.
 *
 * @param[in,out] service Active direct updater service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_updater_direct_service_run(WheelUpdaterDirectService *service, uint32_t now_ms) {
    if (service == NULL ||
        (!wheel_updater_bridge_active(&service->bridge) && !service->operation_pending)) {
        return;
    }
    WheelUpdaterIo io = poll_operation(service, now_ms);
    WheelUpdaterOperation operation = wheel_updater_bridge_step(&service->bridge, io);
    if (!service->operation_pending) {
        start_operation(service, operation);
    }
}

/**
 * @brief Takes one complete raw-link updater response.
 *
 * Delegates retained-response ownership to the common updater protocol service.
 *
 * @param[in,out] service Direct updater service holding a response.
 * @param[out] response Complete updater response bytes.
 * @param[out] length Complete response length.
 * @return true when a response was available; otherwise false.
 */
bool wheel_updater_direct_service_take_response(WheelUpdaterDirectService *service,
                                                const uint8_t **response, uint8_t *length) {
    if (service == NULL ||
        !wheel_updater_bridge_take_response(&service->bridge, response, length)) {
        return false;
    }
    if (!service->operation_pending) {
        service->pending_operation = WHEEL_UPDATER_OPERATION_NONE;
        service->pending_length = 0;
        platform_serial_link_direct_clear();
    }
    return true;
}

/**
 * @brief Reports whether the raw updater service owns an exchange.
 *
 * Includes queued and pending byte operations plus untaken complete responses.
 *
 * @param[in] service Direct updater service to inspect.
 * @return true while an updater exchange is active; otherwise false.
 */
bool wheel_updater_direct_service_active(const WheelUpdaterDirectService *service) {
    return service != NULL &&
           (service->operation_pending || wheel_updater_bridge_active(&service->bridge));
}
