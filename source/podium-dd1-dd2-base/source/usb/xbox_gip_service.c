#include "usb/xbox_gip_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/xbox_gip_discovery.h"
#include "usb/xbox_gip_metadata_download.h"
#include "usb/xbox_gip_response.h"
#include "usb/xbox_gip_session.h"

enum {
    XBOX_GIP_SCRIPT_SAMPLE_PACKET = 0x0b,
    XBOX_GIP_SCRIPT_INPUT_PACKET = 0x0e,
};

/**
 * @brief Classifies an Xbox GIP force-feedback application packet.
 *
 * Accepts the four complete script-system packet types used for sample updates, slot control,
 * script uploads, and scheduled live inputs.
 *
 * @param[in] request Received Xbox GIP endpoint packet.
 * @return True when the packet belongs to the force-feedback application path.
 */
static bool
is_force_feedback_application_packet(const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    return request[0] >= XBOX_GIP_SCRIPT_SAMPLE_PACKET &&
           request[0] <= XBOX_GIP_SCRIPT_INPUT_PACKET;
}

/**
 * @brief Emits a response for accepted Xbox GIP session actions.
 *
 * Builds ready responses with an advanced sequence and transfer-status responses with the current
 * sequence, matching their distinct sequence behavior.
 *
 * @param[in,out] service Active GIP service.
 * @param[in] request Request that produced the session actions.
 * @param[in] actions Accepted session actions.
 * @param[out] response Destination for the response packet.
 * @return Response length, or zero when the actions do not require a response.
 */
static uint8_t emit_session_response(UsbXboxGipService *service, const uint8_t request[5],
                                     UsbXboxGipSessionAction actions,
                                     uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    if ((actions & USB_XBOX_GIP_SESSION_ACTION_SEND_READY) != 0) {
        uint8_t sequence = usb_xbox_gip_sequence_take(&service->next_sequence);
        usb_xbox_gip_ready_response_encode(sequence, response);
        return USB_XBOX_GIP_READY_RESPONSE_SIZE;
    }
    if ((actions & USB_XBOX_GIP_SESSION_ACTION_SEND_TRANSFER_STATUS) != 0) {
        usb_xbox_gip_transfer_status_response_encode(service->next_sequence, request, response);
        return USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE;
    }
    return 0;
}

/**
 * @brief Services an active Xbox GIP metadata download.
 *
 * Accepts matching acknowledgement packets and emits the next transfer packet when its
 * acknowledgement boundary permits progress.
 *
 * @param[in,out] service Active GIP service.
 * @param[in] identity Identity data containing the metadata document.
 * @param[in] request Current received packet.
 * @param[out] response Destination for the next metadata packet.
 * @return Metadata response length, or zero while waiting for acknowledgement.
 */
static uint8_t service_metadata(UsbXboxGipService *service,
                                const UsbXboxGipServiceIdentity *identity,
                                const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE],
                                uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    if (service->metadata_pending) {
        usb_xbox_gip_session_finish_metadata(&service->session);
        service->metadata_pending = false;
    }
    if (service->metadata_download.awaiting_acknowledgement) {
        usb_xbox_gip_metadata_download_acknowledge(&service->metadata_download, request);
        return 0;
    }

    uint8_t response_length = usb_xbox_gip_metadata_download_next(&service->metadata_download,
                                                                  identity->metadata, response);
    if (response_length == 0) {
        return 0;
    }
    if (service->metadata_download.complete) {
        service->metadata_active = false;
    }
    return response_length;
}

/**
 * @brief Initializes the Xbox GIP endpoint service.
 *
 * Starts discovery and session state with response sequence 1 and no active metadata transfer.
 *
 * @param[out] service GIP service state to initialize.
 */
void usb_xbox_gip_service_init(UsbXboxGipService *service) {
    *service = (UsbXboxGipService){.next_sequence = 1};
    usb_xbox_gip_discovery_init(&service->discovery);
    usb_xbox_gip_session_init(&service->session);
}

/**
 * @brief Services one Xbox GIP endpoint cycle.
 *
 * Runs discovery, starts and advances metadata transfer, applies session commands, and emits at
 * most one response packet for the cycle.
 *
 * @param[in,out] service Active GIP service.
 * @param[in] identity Base, wheel, digest, and metadata identity inputs.
 * @param[in] request Current 64-byte request packet, or an all-zero packet when none was received.
 * @param[in] now Current monotonic time in milliseconds.
 * @param[out] response Destination for a response packet.
 * @return Session actions, response length, and application-output classification for the cycle.
 */
UsbXboxGipServiceResult
usb_xbox_gip_service_poll(UsbXboxGipService *service, const UsbXboxGipServiceIdentity *identity,
                          const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE], uint32_t now,
                          uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    UsbXboxGipServiceResult result = {
        .application_output = is_force_feedback_application_packet(request),
    };
    if (service->metadata_active) {
        result.response_length = service_metadata(service, identity, request, response);
        return result;
    }

    if (service->session.state == USB_XBOX_GIP_SESSION_DISCOVERY) {
        UsbXboxGipDiscoveryAction action =
            usb_xbox_gip_discovery_poll(&service->discovery, request[0], now);
        if (action == USB_XBOX_GIP_DISCOVERY_DIGEST) {
            uint8_t sequence = usb_xbox_gip_sequence_take(&service->next_sequence);
            usb_xbox_gip_digest_response_encode(identity->variant, identity->wheel_mode, sequence,
                                                identity->digest, response);
            result.response_length = USB_XBOX_GIP_DIGEST_RESPONSE_SIZE;
        } else if (action == USB_XBOX_GIP_DISCOVERY_METADATA) {
            usb_xbox_gip_session_begin_metadata(&service->session);
            usb_xbox_gip_metadata_download_init(&service->metadata_download,
                                                service->next_sequence);
            service->metadata_pending = true;
            service->metadata_active = true;
        } else if (action == USB_XBOX_GIP_DISCOVERY_SESSION_COMMAND) {
            result.session_actions = usb_xbox_gip_session_handle(&service->session, request);
            result.response_length =
                emit_session_response(service, request, result.session_actions, response);
        }
        return result;
    }

    result.session_actions = usb_xbox_gip_session_handle(&service->session, request);
    result.response_length =
        emit_session_response(service, request, result.session_actions, response);
    if ((result.session_actions & USB_XBOX_GIP_SESSION_ACTION_RESET_FORCE_FEEDBACK) != 0) {
        usb_xbox_gip_session_finish_force_feedback_reset(&service->session);
    }
    return result;
}
