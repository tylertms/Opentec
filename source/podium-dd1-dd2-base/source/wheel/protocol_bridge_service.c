#include "wheel/protocol_bridge_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Internal callback endpoint, owner, and payload selectors. */
enum {
    WHEEL_PROTOCOL_BRIDGE_OWNER = 0x42,           /**< Command-transport owner identifier. */
    WHEEL_PROTOCOL_BRIDGE_CALLBACK_OFFSET = 0x0d, /**< Callback write offset. */
};

/** @brief Callback token written to the selected endpoint. */
static const uint8_t callback[] = {0xfa, 0x05};

/**
 * @brief Tests whether a report identifier was selected by startup negotiation.
 *
 * @param[in] report_id Candidate callback report identifier.
 * @return True for the standard or extended negotiated identifier.
 */
static bool valid_report_id(uint8_t report_id) {
    return report_id == WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD ||
           report_id == WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED;
}

/**
 * @brief Initializes the attached-wheel protocol callback service.
 *
 * Attaches the shared command transport and leaves the callback request idle with no retained
 * report identifier or acknowledgement.
 *
 * @param[out] service Protocol callback service to initialize.
 * @param[in] transport Shared attached-wheel command transport to attach.
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
 * Starts one request through the negotiated report identifier. Invalid identifiers and requests
 * made while another callback is active are rejected.
 *
 * @param[in,out] service Idle protocol callback service accepting the request.
 * @param[in] report_id Negotiated callback report identifier, either 0x15 or 0x16.
 * @return True when a new callback request started; otherwise false.
 */
bool wheel_protocol_bridge_service_request(WheelProtocolBridgeService *service, uint8_t report_id) {
    if (service == NULL || service->transport == NULL ||
        service->phase != WHEEL_PROTOCOL_BRIDGE_IDLE || !valid_report_id(report_id)) {
        return false;
    }
    service->report_id = report_id;
    service->acknowledged = false;
    service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_READY;
    return true;
}

/**
 * @brief Advances the attached-wheel protocol bridge callback.
 *
 * Writes callback token 0x05FA at offset 0x0D through the retained negotiated report identifier and
 * latches successful completion for the runtime transition. A rejected transfer ends the request
 * without retrying another identifier.
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
        service->phase = WHEEL_PROTOCOL_BRIDGE_IDLE;
        return;
    }

    command_transport_claim(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER);
    if (!command_transport_is_owner(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER) ||
        command_transport_poll(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER) !=
            COMMAND_TRANSPORT_COMPLETE) {
        return;
    }
    if (command_transport_queue_write_to(service->transport, WHEEL_PROTOCOL_BRIDGE_OWNER,
                                         service->report_id, WHEEL_PROTOCOL_BRIDGE_CALLBACK_OFFSET,
                                         callback,
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
