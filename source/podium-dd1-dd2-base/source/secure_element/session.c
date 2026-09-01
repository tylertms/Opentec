#include "secure_element/session.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "secure_element/a71ch.h"
#include "secure_element/bus.h"

/**
 * @brief Initializes the A71CH SCI2C session service.
 *
 * Clears transaction ownership and leaves the session dormant until explicitly requested.
 *
 * @param[out] service Session service state.
 */
void a71ch_session_service_init(A71chSessionService *service) {
    *service = (A71chSessionService){0};
}

/**
 * @brief Starts an A71CH SCI2C session sequence.
 *
 * Initializes a new protocol sequence unless the service is already running, in which case the
 * active sequence continues without interruption.
 *
 * @param[in,out] service Session service state.
 */
void a71ch_session_service_start(A71chSessionService *service) {
    if (service->status == A71CH_SESSION_SERVICE_RUNNING) {
        return;
    }

    a71ch_session_init(&service->session);
    service->status = A71CH_SESSION_SERVICE_RUNNING;
    service->transfer_active = false;
}

/**
 * @brief Applies one completed A71CH session transaction.
 *
 * Releases the shared bus and converts a successful response into the length, control, and payload
 * fields consumed by the session.
 *
 * @param[in,out] service Session service state and response buffer.
 * @param[in] succeeded True when the auxiliary-bus transaction succeeded.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void complete_transfer(A71chSessionService *service, bool succeeded, uint32_t now_ms) {
    platform_aux_bus_clear();
    service->transfer_active = false;
    if (!succeeded) {
        return;
    }

    const A71chControlRequest *request = a71ch_control_request_lookup(service->active_command);
    service->response_view = (A71chSessionResponse){
        .declared_length = request->response_length == 0 ? 0 : service->response[0],
        .status = request->response_length < 2 ? 0 : service->response[1],
        .payload = request->response_length <= 2 ? 0 : &service->response[2],
        .payload_length = request->response_length <= 2 ? 0 : request->response_length - 2,
    };
    a71ch_session_accept(&service->session, service->active_command,
                         request->response_length == 0 ? 0 : &service->response_view, now_ms);
    if (service->session.complete) {
        service->status = A71CH_SESSION_SERVICE_COMPLETE;
    }
}

/**
 * @brief Advances the A71CH SCI2C session service.
 *
 * Waits for an owned transaction, applies its terminal result, and starts the next protocol
 * command only while the shared auxiliary bus is idle. A failed start or transaction remains
 * available for retry on a later call.
 *
 * @param[in,out] service Session service state and response buffer.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void a71ch_session_service_run(A71chSessionService *service, uint32_t now_ms) {
    if (service->status != A71CH_SESSION_SERVICE_RUNNING) {
        return;
    }

    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (service->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        complete_transfer(service, bus_status == PLATFORM_AUX_BUS_SUCCEEDED, now_ms);
        return;
    }
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return;
    }

    if (!a71ch_session_current(&service->session, now_ms, &service->active_command)) {
        return;
    }

    const A71chControlRequest *request = a71ch_control_request_lookup(service->active_command);
    service->transfer_active = a71ch_bus_start(
        service->active_command, request->response_length == 0 ? 0 : service->response);
}

/**
 * @brief Returns the current A71CH session service status.
 *
 * Reports whether the session is dormant, in progress, or complete without exposing transaction
 * ownership details.
 *
 * @param[in] service Session service state.
 * @return Current session service status.
 */
A71chSessionServiceStatus a71ch_session_service_status(const A71chSessionService *service) {
    return service->status;
}
