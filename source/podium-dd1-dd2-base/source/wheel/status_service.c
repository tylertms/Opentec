#include "wheel/status_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/time.h"
#include "serial/message.h"
#include "serial/service.h"

enum {
    WHEEL_STATUS_MESSAGE_TYPE = 5,
    WHEEL_STATUS_RESPONSE_SIZE = 15,
    WHEEL_STATUS_MARKER_OFFSET = 14,
    WHEEL_STATUS_MARKER = 0xaa,
    WHEEL_STATUS_POLL_INTERVAL_MS = 1000,
};

/**
 * @brief Decodes a little-endian 16-bit wheel-status value.
 *
 * Combines two consecutive response bytes with the least-significant byte first.
 *
 * @param[in] data First response byte.
 * @return Decoded 16-bit value.
 */
static uint16_t decode_u16(const uint8_t data[2]) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

/**
 * @brief Decodes a little-endian 32-bit wheel-status value.
 *
 * Combines four consecutive response bytes with the least-significant byte first.
 *
 * @param[in] data First response byte.
 * @return Decoded 32-bit value.
 */
static uint32_t decode_u32(const uint8_t data[4]) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
           (uint32_t)data[3] << 24;
}

/**
 * @brief Applies one complete attached-wheel status response.
 *
 * Decodes the first thirteen bytes into the logical status snapshot, ignores the reserved byte,
 * and latches a marked response when the final byte is 0xAA.
 *
 * @param[in,out] service Wheel-status service receiving the response.
 * @param[in] response Fifteen-byte type-five response payload.
 */
static void accept_response(WheelStatusService *service,
                            const uint8_t response[WHEEL_STATUS_RESPONSE_SIZE]) {
    service->snapshot.status_high = response[0];
    service->snapshot.status_low = response[1];
    service->snapshot.accessory_value = decode_u16(response + 2);
    service->snapshot.runtime_seconds = decode_u32(response + 4);
    service->snapshot.runtime_counter = decode_u32(response + 8);
    service->snapshot.trailing_status = response[12];
    if (response[WHEEL_STATUS_MARKER_OFFSET] == WHEEL_STATUS_MARKER) {
        service->marked_response_ready = true;
    }
}

/**
 * @brief Initializes attached-wheel status polling.
 *
 * Clears the current snapshot, marker, completion flag, and poll deadline, then attaches the shared
 * serial service used for type-five requests.
 *
 * @param[out] service Wheel-status service to initialize.
 * @param[in,out] transport Shared attached-device serial service.
 */
void wheel_status_service_init(WheelStatusService *service, SerialService *transport) {
    if (service == 0) {
        return;
    }
    *service = (WheelStatusService){
        .transport = transport,
    };
}

/**
 * @brief Advances attached-wheel status polling.
 *
 * Applies a completed fifteen-byte type-five response, releases failed requests, and submits the
 * next one-byte marker request no more than once per second when the scheduler grants a slot.
 *
 * @param[in,out] service Wheel-status service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] start_allowed Allows a due status request to claim the shared serial service.
 */
void wheel_status_service_run(WheelStatusService *service, uint32_t now_ms, bool start_allowed) {
    if (service == 0 || service->transport == 0 ||
        service->transport->status == SERIAL_SERVICE_PENDING) {
        return;
    }
    if (service->transport->status != SERIAL_SERVICE_IDLE &&
        service->transport->request_type != WHEEL_STATUS_MESSAGE_TYPE) {
        return;
    }
    if (service->transport->status == SERIAL_SERVICE_SUCCEEDED) {
        const SerialMessageAssembly *response = serial_service_response(service->transport);
        if (response != 0 && response->length == WHEEL_STATUS_RESPONSE_SIZE) {
            accept_response(service, response->data);
        }
        serial_service_release(service->transport);
    } else if (service->transport->status == SERIAL_SERVICE_FAILED) {
        serial_service_release(service->transport);
    }

    if (!start_allowed ||
        (service->poll_deadline_active && !platform_time_reached(now_ms, service->next_poll_ms))) {
        return;
    }
    if (serial_service_start(service->transport, WHEEL_STATUS_MESSAGE_TYPE,
                             &service->request_marker, 1, now_ms)) {
        service->request_marker = 0;
        service->next_poll_ms = now_ms + WHEEL_STATUS_POLL_INTERVAL_MS;
        service->poll_deadline_active = true;
    }
}

/**
 * @brief Marks the next attached-wheel status request.
 *
 * Sets the one-byte request payload to 0xAA and makes the request immediately eligible so a
 * matching response can signal completion.
 *
 * @param[in,out] service Wheel-status service whose next request is marked.
 */
void wheel_status_service_mark_next_request(WheelStatusService *service) {
    if (service != 0) {
        service->request_marker = WHEEL_STATUS_MARKER;
        service->poll_deadline_active = false;
    }
}

/**
 * @brief Takes the marked wheel-status response signal.
 *
 * Clears the latched signal after reporting it once.
 *
 * @param[in,out] service Wheel-status service to inspect.
 * @return True once for each received response ending in marker 0xAA.
 */
bool wheel_status_service_take_marked_response(WheelStatusService *service) {
    if (service == 0 || !service->marked_response_ready) {
        return false;
    }
    service->marked_response_ready = false;
    return true;
}

/**
 * @brief Returns the most recent attached-wheel status snapshot.
 *
 * Exposes the decoded status bytes, accessory value, two runtime counters, and trailing status.
 *
 * @param[in] service Wheel-status service to inspect.
 * @return Current snapshot, or null when the service is unavailable.
 */
const WheelStatusSnapshot *wheel_status_service_snapshot(const WheelStatusService *service) {
    return service != 0 ? &service->snapshot : 0;
}
