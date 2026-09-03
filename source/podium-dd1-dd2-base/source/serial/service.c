#include "serial/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/serial_link.h"
#include "platform/time.h"
#include "serial/session.h"

/** @brief Internal serial request retry constants. */
enum {
    SERIAL_SERVICE_TIMEOUT_MS = 10,  /**< Response window for one packet attempt in milliseconds. */
    SERIAL_SERVICE_MAX_ATTEMPTS = 5, /**< Maximum timeout or malformed-packet retries. */
};

/**
 * @brief Identifies a valid framed packet with an unsupported logical type.
 *
 * Valid control packets use types zero and one, while logical messages use types two through five.
 *
 * @param[in] input Encoded framed packet to inspect.
 * @return True when the packet is valid but its logical type is unsupported.
 */
static bool is_unsupported_packet(const uint8_t input[SERIAL_PACKET_SIZE]) {
    if (input == 0 || input[0] != SERIAL_PACKET_START ||
        input[SERIAL_PACKET_SIZE - 1] != SERIAL_PACKET_END ||
        (input[1] & SERIAL_PACKET_TYPE_MASK) <= SERIAL_MESSAGE_LAST_TYPE ||
        !serial_packet_checksum_valid(input)) {
        return false;
    }
    return true;
}

/**
 * @brief Identifies a transport framing failure.
 *
 * Boundary failures use the official periodic recovery hold. Length and checksum failures retain
 * the ordinary synchronization retry path.
 *
 * @param[in] input Encoded framed packet to inspect.
 * @return True when either transport boundary is invalid.
 */
static bool is_framing_failure(const uint8_t input[SERIAL_PACKET_SIZE]) {
    return input[0] != SERIAL_PACKET_START || input[SERIAL_PACKET_SIZE - 1] != SERIAL_PACKET_END;
}

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
    service->packet_pending = true;
    return true;
}

/**
 * @brief Encodes the synchronization packet held during periodic recovery.
 *
 * Leaves the encoded packet in service storage until the physical link accepts it after Timer 6
 * clears the reset-pending flag.
 *
 * @param[in,out] service Serial service with a pending synchronization packet.
 * @return True when a packet was encoded.
 */
static bool defer_next_packet(SerialService *service) {
    if (!serial_session_next_packet(&service->session, service->packet)) {
        return false;
    }
    service->recovery_pending = true;
    return true;
}

/**
 * @brief Starts an encoded synchronization packet after periodic recovery.
 *
 * @param[in,out] service Serial service holding the encoded packet.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void start_recovery_packet(SerialService *service, uint32_t now_ms) {
    if (platform_serial_link_start(service->packet)) {
        service->recovery_pending = false;
        service->packet_pending = true;
        service->deadline_ms = now_ms + SERIAL_SERVICE_TIMEOUT_MS;
    }
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
 * Queues a type-two through type-five message of up to SERIAL_MESSAGE_MAX_SIZE bytes on the
 * shared session and immediately starts its first fixed packet exchange.
 *
 * @param[in,out] service Idle serial service accepting the request.
 * @param[in] type Logical message type from two through five.
 * @param[in] message Complete request message.
 * @param[in] length Request length from one through SERIAL_MESSAGE_MAX_SIZE bytes.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] bounded_attempts Whether transport retries are bounded.
 * @return True when the request and first packet exchange start.
 */
static bool start_request(SerialService *service, uint8_t type, const uint8_t *message,
                          uint16_t length, uint32_t now_ms, bool bounded_attempts) {
    if (service == 0 || service->status != SERIAL_SERVICE_IDLE ||
        !serial_session_queue(&service->session, type, message, length)) {
        return false;
    }
    service->request_type = type;
    service->attempts = 0;
    service->bounded_attempts = bounded_attempts;
    service->status = SERIAL_SERVICE_PENDING;
    if (start_next_packet(service, now_ms)) {
        return true;
    }
    serial_session_finish_transmit(&service->session);
    service->status = SERIAL_SERVICE_FAILED;
    return false;
}

bool serial_service_start(SerialService *service, uint8_t type, const uint8_t *message,
                          uint16_t length, uint32_t now_ms) {
    return start_request(service, type, message, length, now_ms, false);
}

/**
 * @brief Starts a serial request with a bounded retry count.
 *
 * @param[in,out] service Idle serial service.
 * @param[in] type Logical request type.
 * @param[in] message Request bytes.
 * @param[in] length Request length in bytes.
 * @param[in] now_ms Current monotonic time.
 * @return True when the request starts; otherwise false.
 */
bool serial_service_start_wait(SerialService *service, uint8_t type, const uint8_t *message,
                               uint16_t length, uint32_t now_ms) {
    return start_request(service, type, message, length, now_ms, true);
}

/**
 * @brief Advances the shared attached-device request and response exchange.
 *
 * Accepts completed packets, emits required fragment acknowledgements or resynchronization
 * packets, publishes a matching completed logical response, and retries a packet after each
 * ten-millisecond response deadline for up to five retry events. A failed restart or fifth timeout
 * or malformed event fails a bounded request, while a framing failure schedules its recovery
 * synchronization packet after the official periodic hold.
 *
 * @param[in,out] service Serial service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void serial_service_run(SerialService *service, uint32_t now_ms) {
    if (service == 0 || service->status != SERIAL_SERVICE_PENDING) {
        return;
    }
    if (service->recovery_pending) {
        start_recovery_packet(service, now_ms);
        return;
    }
    if (service->packet_pending && platform_serial_link_take_received(service->packet)) {
        service->packet_pending = false;
        SerialSessionResult result = serial_session_accept(&service->session, service->packet);
        bool malformed = result == SERIAL_SESSION_INVALID_PACKET ||
                         result == SERIAL_SESSION_MESSAGE_OVERFLOW;
        bool unsupported = malformed && is_unsupported_packet(service->packet);
        if (malformed && !unsupported) {
            service->error_count++;
            service->attempts++;
        }
        if (malformed) {
            if (!unsupported && is_framing_failure(service->packet)) {
                if (!platform_serial_link_start_periodic_recovery() ||
                    !defer_next_packet(service)) {
                    service->status = SERIAL_SERVICE_FAILED;
                } else if (service->bounded_attempts &&
                           service->attempts >= SERIAL_SERVICE_MAX_ATTEMPTS) {
                    service->recovery_pending = false;
                    service->status = SERIAL_SERVICE_FAILED;
                }
            } else if (!unsupported && service->bounded_attempts &&
                       service->attempts >= SERIAL_SERVICE_MAX_ATTEMPTS) {
                service->status = SERIAL_SERVICE_FAILED;
            } else if (!start_next_packet(service, now_ms)) {
                service->status = SERIAL_SERVICE_FAILED;
            }
            return;
        }
        if (result == SERIAL_SESSION_MESSAGE_COMPLETE) {
            const SerialMessageAssembly *message = serial_session_message(&service->session);
            bool matches = message != 0 && message->type == service->request_type;
            if (matches) {
                service->response = *message;
            }
            serial_session_consume_message(&service->session);
            serial_session_finish_transmit(&service->session);
            service->status = matches ? SERIAL_SERVICE_SUCCEEDED : SERIAL_SERVICE_FAILED;
            return;
        }
        if (!start_next_packet(service, now_ms)) {
            service->status = SERIAL_SERVICE_FAILED;
        }
        return;
    }
    if (service->packet_pending && platform_time_reached(now_ms, service->deadline_ms + 1u)) {
        service->error_count++;
        service->attempts++;
        platform_serial_link_reset();
        if ((!service->bounded_attempts || service->attempts < SERIAL_SERVICE_MAX_ATTEMPTS) &&
            platform_serial_link_start(service->packet)) {
            service->deadline_ms = now_ms + SERIAL_SERVICE_TIMEOUT_MS;
        } else if (service->bounded_attempts && service->attempts >= SERIAL_SERVICE_MAX_ATTEMPTS) {
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
    return service != 0 && service->status == SERIAL_SERVICE_SUCCEEDED ? &service->response : 0;
}

/**
 * @brief Returns the attached-device serial error count.
 *
 * Reports the cumulative number of expired response windows and rejected or overflowing incoming
 * packets since serial service initialization.
 *
 * @param[in] service Serial service to inspect.
 * @return Cumulative transport error count, or zero when the service is unavailable.
 */
uint32_t serial_service_error_count(const SerialService *service) {
    return service != 0 ? service->error_count : 0;
}

/**
 * @brief Cancels the current attached-device request.
 *
 * Stops a pending physical transfer, clears logical transmit and receive state, and returns the
 * service to idle while preserving the shared packet sequence and cumulative error count.
 *
 * @param[in,out] service Serial service to cancel.
 */
void serial_service_cancel(SerialService *service) {
    if (service == 0 || service->status == SERIAL_SERVICE_IDLE) {
        return;
    }
    if (service->status == SERIAL_SERVICE_PENDING) {
        platform_serial_link_reset();
    }
    serial_session_consume_message(&service->session);
    serial_session_finish_transmit(&service->session);
    service->request_type = 0;
    service->attempts = 0;
    service->packet_pending = false;
    service->recovery_pending = false;
    service->response = (SerialMessageAssembly){0};
    service->status = SERIAL_SERVICE_IDLE;
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
    service->packet_pending = false;
    service->recovery_pending = false;
    service->response = (SerialMessageAssembly){0};
    service->status = SERIAL_SERVICE_IDLE;
}
