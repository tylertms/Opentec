#include "wheel/protocol_bridge_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transfer/command.h"

enum {
    WHEEL_PROTOCOL_BRIDGE_ENDPOINT_COUNT = 2,
    WHEEL_PROTOCOL_BRIDGE_OWNER = 0x42,
    WHEEL_PROTOCOL_BRIDGE_CALLBACK_OFFSET = 0x0d,
};

static const uint8_t endpoints[WHEEL_PROTOCOL_BRIDGE_ENDPOINT_COUNT] = {0x15, 0x16};
static const uint8_t callback[] = {0xfa, 0x05};

/**
 * @brief Initializes the attached-wheel protocol callback service.
 *
 * Attaches the shared command transport and leaves the callback request idle with no retained
 * acknowledgement.
 *
 * @param[out] service Protocol callback service to initialize.
 * @param[in,out] transport Shared attached-wheel command transport.
 */
void wheel_protocol_bridge_service_init(WheelProtocolBridgeService *service,
                                        CommandTransport *transport) {
    if (service != NULL) {
        *service = (WheelProtocolBridgeService){.transport = transport};
    }
}

/**
 * @brief Requests the attached-wheel protocol bridge callback.
 *
 * Starts with command endpoint 0x15 and retains one request while the service tries the supported
 * attached-wheel endpoints.
 *
 * @param[in,out] service Idle protocol callback service accepting the request.
 * @return True when a new callback request started; otherwise false.
 */
bool wheel_protocol_bridge_service_request(WheelProtocolBridgeService *service) {
    if (service == NULL || service->transport == NULL ||
        service->phase != WHEEL_PROTOCOL_BRIDGE_IDLE) {
        return false;
    }
    service->endpoint_index = 0;
    service->acknowledged = false;
    service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_READY;
    return true;
}

/**
 * @brief Advances the attached-wheel protocol bridge callback.
 *
 * Writes callback token 0x05FA at offset 0x0D through endpoint 0x15, retries endpoint 0x16 after a
 * rejected transfer, and latches successful completion for the runtime transition.
 *
 * @param[in,out] service Active protocol callback service.
 */
void wheel_protocol_bridge_service_run(WheelProtocolBridgeService *service) {
    if (service == NULL || service->transport == NULL ||
        service->phase == WHEEL_PROTOCOL_BRIDGE_IDLE) {
        return;
    }

    if (service->phase == WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING) {
        CommandTransportResult result =
            command_transport_poll(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER);
        if (result == COMMAND_TRANSPORT_BUSY) {
            return;
        }
        command_transport_release(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->acknowledged = true;
            service->phase = WHEEL_PROTOCOL_BRIDGE_IDLE;
            return;
        }
        service->endpoint_index++;
        service->phase = service->endpoint_index < WHEEL_PROTOCOL_BRIDGE_ENDPOINT_COUNT
                             ? WHEEL_PROTOCOL_BRIDGE_WRITE_READY
                             : WHEEL_PROTOCOL_BRIDGE_IDLE;
        return;
    }

    command_transport_claim(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER);
    if (!command_transport_is_owner(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER) ||
        command_transport_poll(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER) !=
            COMMAND_TRANSPORT_COMPLETE) {
        return;
    }
    if (command_transport_queue_write_to(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER,
                                         endpoints[service->endpoint_index],
                                         WHEEL_PROTOCOL_BRIDGE_CALLBACK_OFFSET, callback,
                                         sizeof(callback)) == COMMAND_TRANSPORT_COMPLETE) {
        service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING;
    }
}

/**
 * @brief Takes a completed attached-wheel protocol callback acknowledgement.
 *
 * Clears the one-shot completion latch after exposing it to the runtime transition.
 *
 * @param[in,out] service Protocol callback service holding the completion latch.
 * @return True once after an accepted callback write; otherwise false.
 */
bool wheel_protocol_bridge_service_take_acknowledgement(WheelProtocolBridgeService *service) {
    if (service == NULL || !service->acknowledged) {
        return false;
    }
    service->acknowledged = false;
    return true;
}
