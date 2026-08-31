#include "serial/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/serial_link.h"
#include "platform/time.h"
#include "serial/session.h"

enum {
    SERIAL_SERVICE_TIMEOUT_MS = 10,
    SERIAL_SERVICE_MAX_ATTEMPTS = 5,
};

/**
 * @brief Starts the next scheduled attached-device packet exchange.
 *
 * Encodes the session's pending data or control packet and starts its ten-millisecond response
 * window on the shared UART3 link.
 *
 * @param[in,out] service Serial service with a scheduled packet.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when no packet was scheduled or the scheduled exchange started.
 */
static bool start_next_packet(SerialService *service, uint32_t now_ms) {
    if (!serial_session_next_packet(&service->session, service->packet)) {
        return true;
    }
    if (!platform_serial_link_start(service->packet)) {
        return false;
    }
    service->deadline_ms = now_ms + SERIAL_SERVICE_TIMEOUT_MS;
    service->attempts = 1;
    service->packet_pending = true;
    return true;
}

/**
 * @brief Initializes the shared attached-device serial service.
 *
 * Clears logical message state and starts the single packet sequence at zero without starting a
 * physical exchange.
 *
 * @param[out] service Serial service to initialize.
 */
void serial_service_init(SerialService *service) {
    if (service == 0) {
        return;
    }
    *service = (SerialService){0};
    serial_session_init(&service->session);
}

/**
 * @brief Starts one logical attached-device request.
 *
 * Queues a type-two through type-five message on the one shared session and immediately starts its
 * first fixed packet exchange.
 *
 * @param[in,out] service Idle serial service accepting the request.
 * @param[in] type Logical message type from two through five.
 * @param[in] message Complete request message.
 * @param[in] length Request length from one through 512 bytes.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the request and first packet exchange start.
 */
bool serial_service_start(SerialService *service, uint8_t type, const uint8_t *message,
                          uint16_t length, uint32_t now_ms) {
    if (service == 0 || service->status != SERIAL_SERVICE_IDLE ||
        !serial_session_queue(&service->session, type, message, length)) {
        return false;
    }
    service->request_type = type;
    service->status = SERIAL_SERVICE_PENDING;
    if (start_next_packet(service, now_ms)) {
        return true;
    }
    serial_session_finish_transmit(&service->session);
    service->status = SERIAL_SERVICE_FAILED;
    return false;
}

/**
 * @brief Advances the shared attached-device request and response exchange.
 *
 * Accepts completed packets, emits required fragment acknowledgements or resynchronization
 * packets, publishes a matching completed logical response, and retransmits a packet at each
 * ten-millisecond deadline before the fifth missed response fails the request.
 *
 * @param[in,out] service Serial service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void serial_service_run(SerialService *service, uint32_t now_ms) {
    if (service == 0 || service->status != SERIAL_SERVICE_PENDING) {
        return;
    }
    if (service->packet_pending && platform_serial_link_take_received(service->packet)) {
        service->packet_pending = false;
        SerialSessionResult result = serial_session_accept(&service->session, service->packet);
        if (result == SERIAL_SESSION_INVALID_PACKET || result == SERIAL_SESSION_MESSAGE_OVERFLOW) {
            service->error_count++;
        }
        if (result == SERIAL_SESSION_MESSAGE_COMPLETE) {
            const SerialMessageAssembly *message = serial_session_message(&service->session);
            service->status = message != 0 && message->type == service->request_type
                                  ? SERIAL_SERVICE_SUCCEEDED
                                  : SERIAL_SERVICE_FAILED;
            return;
        }
        if (!start_next_packet(service, now_ms)) {
            service->status = SERIAL_SERVICE_FAILED;
        }
        return;
    }
    if (service->packet_pending && platform_time_reached(now_ms, service->deadline_ms + 1u)) {
        service->error_count++;
        platform_serial_link_reset();
        if (service->attempts < SERIAL_SERVICE_MAX_ATTEMPTS &&
            platform_serial_link_start(service->packet)) {
            service->attempts++;
            service->deadline_ms = now_ms + SERIAL_SERVICE_TIMEOUT_MS;
        } else {
            service->packet_pending = false;
            service->status = SERIAL_SERVICE_FAILED;
        }
    }
}

/**
 * @brief Returns the completed attached-device response.
 *
 * Exposes the assembled response only after a matching logical message completes.
 *
 * @param[in] service Serial service to inspect.
 * @return Completed response, or null while no matching response is available.
 */
const SerialMessageAssembly *serial_service_response(const SerialService *service) {
    return service != 0 && service->status == SERIAL_SERVICE_SUCCEEDED
               ? serial_session_message(&service->session)
               : 0;
}

/**
 * @brief Returns the attached-device serial error count.
 *
 * Reports the cumulative number of expired response windows and rejected incoming packets since
 * serial service initialization.
 *
 * @param[in] service Serial service to inspect.
 * @return Cumulative transport error count, or zero when the service is unavailable.
 */
uint32_t serial_service_error_count(const SerialService *service) {
    return service != 0 ? service->error_count : 0;
}

/**
 * @brief Releases a completed attached-device request.
 *
 * Consumes any assembled response and clears the outgoing message while preserving the shared
 * packet sequence for the next logical request.
 *
 * @param[in,out] service Completed or failed serial service to release.
 */
void serial_service_release(SerialService *service) {
    if (service == 0 || service->status == SERIAL_SERVICE_PENDING) {
        return;
    }
    serial_session_consume_message(&service->session);
    serial_session_finish_transmit(&service->session);
    service->request_type = 0;
    service->attempts = 0;
    service->status = SERIAL_SERVICE_IDLE;
}
