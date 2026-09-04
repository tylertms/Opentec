#include "wheel/protocol_bridge_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Internal callback endpoint, owner, and payload selectors. */
enum {
    WHEEL_PROTOCOL_BRIDGE_CALLBACK_OFFSET = 0x0d, /**< Callback write offset. */
    WHEEL_PROTOCOL_BRIDGE_WAIT_LIMIT = 500,       /**< Maximum busy polls before recovery. */
};

/** @brief Callback token written to the selected endpoint. */
static const uint8_t callback[] = {0xfa, 0x05};

/**
 * @brief Initializes the attached-wheel protocol callback service.
 *
 * Attaches the shared command transport and leaves the callback request idle with no retained
 * report identifier, wait count, or acknowledgement.
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
 * Starts one request through the negotiated report identifier. A zero identifier is rejected
 * because zero denotes an unowned command transport. Requests made before a write is queued
 * refresh the target, while requests made during a transfer or recovery are coalesced.
 *
 * @param[in,out] service Idle protocol callback service accepting the request.
 * @param[in] report_id Nonzero negotiated callback report identifier.
 * @return True when the request was accepted or coalesced; otherwise false.
 */
bool wheel_protocol_bridge_service_request(WheelProtocolBridgeService *service, uint8_t report_id) {
    if (service == NULL || service->transport == NULL || report_id == 0) {
        return false;
    }
    if (service->phase == WHEEL_PROTOCOL_BRIDGE_IDLE) {
        service->report_id = report_id;
        service->wait_calls = 0;
        service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_READY;
    } else if (service->phase == WHEEL_PROTOCOL_BRIDGE_WRITE_READY) {
        service->report_id = report_id;
    }
    return true;
}

/**
 * @brief Drains a failed callback transfer and enters startup recovery.
 *
 * Waits until the shared transport can be polled, consumes the failed completion, releases the
 * negotiated owner, and schedules the next callback attempt.
 *
 * @param[in,out] service Callback service entering startup recovery.
 * @return True when the transport was recovered; otherwise false while another owner is active.
 */
static bool recover_failed_write(WheelProtocolBridgeService *service) {
    if (command_transport_poll(service->transport, service->report_id) ==
        COMMAND_TRANSPORT_BUSY) {
        return false;
    }
    command_transport_release(service->transport, service->report_id);
    service->wait_calls = 0;
    service->phase = WHEEL_PROTOCOL_BRIDGE_STARTUP_RECOVERY;
    return true;
}

/**
 * @brief Advances the attached-wheel protocol bridge callback.
 *
 * Writes callback token 0x05FA at offset 0x0D through the retained negotiated report identifier and
 * latches successful completion for the runtime transition. A rejected or stalled transfer enters
 * an explicit error phase, resets the endpoint transport, and retries after startup recovery.
 *
 * @param[in,out] service Active protocol callback service.
 */
void wheel_protocol_bridge_service_run(WheelProtocolBridgeService *service) {
    if (service == NULL || service->transport == NULL) {
        return;
    }

    if (service->phase == WHEEL_PROTOCOL_BRIDGE_IDLE) {
        return;
    }

    if (service->phase == WHEEL_PROTOCOL_BRIDGE_WRITE_ERROR) {
        (void)recover_failed_write(service);
        return;
    }
    if (service->phase == WHEEL_PROTOCOL_BRIDGE_STARTUP_RECOVERY) {
        service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_READY;
        return;
    }

    if (service->phase == WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING) {
        CommandTransportResult result =
            command_transport_poll(service->transport, service->report_id);
        if (result == COMMAND_TRANSPORT_BUSY) {
            if (++service->wait_calls <= WHEEL_PROTOCOL_BRIDGE_WAIT_LIMIT) {
                return;
            }
            command_transport_fail(service->transport);
            service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_ERROR;
            return;
        }
        service->wait_calls = 0;
        command_transport_release(service->transport, service->report_id);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->acknowledged = true;
            service->phase = WHEEL_PROTOCOL_BRIDGE_IDLE;
            return;
        }
        service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_ERROR;
        return;
    }

    command_transport_claim(service->transport, service->report_id);
    if (!command_transport_is_owner(service->transport, service->report_id) ||
        command_transport_poll(service->transport, service->report_id) !=
            COMMAND_TRANSPORT_COMPLETE) {
        return;
    }
    if (command_transport_queue_write_to(service->transport, service->report_id, service->report_id,
                                         WHEEL_PROTOCOL_BRIDGE_CALLBACK_OFFSET, callback,
                                         sizeof(callback)) == COMMAND_TRANSPORT_COMPLETE) {
        service->wait_calls = 0;
        service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING;
    } else {
        service->phase = WHEEL_PROTOCOL_BRIDGE_WRITE_ERROR;
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
