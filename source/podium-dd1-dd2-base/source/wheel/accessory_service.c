#include "wheel/accessory_service.h"

#include <stddef.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/accessory.h"

enum {
    WHEEL_ACCESSORY_SERVICE_OWNER = 0x44,
    WHEEL_ACCESSORY_TARGET = 0xf0,
    WHEEL_ACCESSORY_STATUS_OFFSET = 0,
    WHEEL_ACCESSORY_VERSION_OFFSET = 1,
    WHEEL_ACCESSORY_TYPE_OFFSET = 7,
};

/**
 * @brief Decodes the accessory version response.
 *
 * Combines four consecutive response bytes with the least-significant byte first.
 *
 * @param[in] bytes Four-byte version response.
 * @return Decoded accessory version.
 */
static uint32_t decode_version(const uint8_t bytes[4]) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

/**
 * @brief Initializes attached wheel accessory polling.
 *
 * Clears retained response bytes, starts at the status request, and initializes the logical
 * accessory identity as disconnected.
 *
 * @param[out] service Accessory service to initialize.
 */
void wheel_accessory_service_init(WheelAccessoryService *service) {
    if (service == NULL) {
        return;
    }
    *service = (WheelAccessoryService){0};
    wheel_accessory_init(&service->accessory);
}

/**
 * @brief Completes the active accessory identity request.
 *
 * Advances successful status, version, and extended accessory-type reads. A successful version
 * request updates the logical identity. Failed transfers retry the same stage while preserving
 * the last accepted identity.
 *
 * @param[in,out] service Accessory service awaiting a result.
 * @param[in,out] transport Shared command transport carrying the request.
 */
static void finish_request(WheelAccessoryService *service, CommandTransport *transport) {
    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }

    command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    service->request_pending = false;
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return;
    }
    if (service->accessory_type_stage) {
        service->accessory.accessory_type = service->accessory_type_byte;
        service->accessory_type_stage = false;
    } else if (service->version_stage) {
        wheel_accessory_apply_probe(&service->accessory, (int8_t)service->status_byte,
                                    decode_version(service->version_bytes));
        service->version_stage = false;
        if (service->accessory.kind == WHEEL_ACCESSORY_EXTENDED) {
            service->accessory_type_stage = true;
        } else {
            service->accessory.accessory_type = 0;
        }
    } else {
        service->version_stage = true;
    }
}

/**
 * @brief Queues the current accessory identity request.
 *
 * Reads one signed status byte from target 0xF0 offset zero, followed by four version bytes from
 * offset one and the extended accessory type from offset seven. The service waits when another
 * command owner is active.
 *
 * @param[in,out] service Accessory service selecting a request.
 * @param[in,out] transport Shared command transport accepting the request.
 */
static void start_request(WheelAccessoryService *service, CommandTransport *transport) {
    command_transport_claim(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    if (!command_transport_is_owner(transport, WHEEL_ACCESSORY_SERVICE_OWNER)) {
        return;
    }

    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        if (result != COMMAND_TRANSPORT_BUSY) {
            command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
        }
        return;
    }

    if (service->accessory_type_stage) {
        result = command_transport_queue_read_from(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET,
            WHEEL_ACCESSORY_TYPE_OFFSET, &service->accessory_type_byte,
            sizeof(service->accessory_type_byte));
    } else if (!service->version_stage) {
        result = command_transport_queue_read_from(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET,
            WHEEL_ACCESSORY_STATUS_OFFSET, &service->status_byte, sizeof(service->status_byte));
    } else {
        result = command_transport_queue_read_from(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET,
            WHEEL_ACCESSORY_VERSION_OFFSET, service->version_bytes, sizeof(service->version_bytes));
    }
    if (result == COMMAND_TRANSPORT_COMPLETE) {
        service->request_pending = true;
    } else if (result != COMMAND_TRANSPORT_BUSY) {
        command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    }
}

/**
 * @brief Advances attached wheel accessory identity polling.
 *
 * Completes the active request or queues the next status, version, or accessory-type read using
 * the shared command transport.
 *
 * @param[in,out] service Accessory service to advance.
 * @param[in,out] transport Shared command transport used by the service.
 */
void wheel_accessory_service_run(WheelAccessoryService *service, CommandTransport *transport) {
    if (service == NULL || transport == NULL) {
        return;
    }
    if (service->request_pending) {
        finish_request(service, transport);
    } else {
        start_request(service, transport);
    }
}

/**
 * @brief Returns the latest attached wheel accessory identity.
 *
 * Exposes the last accepted protocol kind, model, status, and version.
 *
 * @param[in] service Accessory service to inspect.
 * @return Current accessory identity, or null when the service is unavailable.
 */
const WheelAccessory *wheel_accessory_service_identity(const WheelAccessoryService *service) {
    return service == NULL ? NULL : &service->accessory;
}
