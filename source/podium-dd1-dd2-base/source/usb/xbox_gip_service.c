#include "usb/xbox_gip_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/xbox_gip_discovery.h"
#include "usb/xbox_gip_metadata_download.h"
#include "usb/xbox_gip_response.h"
#include "usb/xbox_gip_session.h"

/** @brief Xbox GIP packet identifiers classified as application output. */
enum {
    XBOX_GIP_COMMAND_PACKET = 0x0a, /**< First application-output packet identifier. */
    XBOX_GIP_VENDOR_PACKET = 0x0f,  /**< Last application-output packet identifier. */
    XBOX_GIP_MEMORY_PACKET = 0x06,  /**< Memory-control packet identifier. */
};

/**
 * @brief Classifies an Xbox GIP force-feedback application packet.
 *
 * Classifies packet identifiers 0x0A through 0x0F as force-feedback application traffic, covering
 * command queries, four script-system packet types, and the vendor tunnel packet.
 *
 * @param[in] request Received Xbox GIP endpoint packet.
 * @return True when the packet belongs to the force-feedback application path.
 */
static bool
is_force_feedback_application_packet(const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    return request[0] >= XBOX_GIP_COMMAND_PACKET && request[0] <= XBOX_GIP_VENDOR_PACKET;
}

/**
 * @brief Classifies a packet-6 request in a memory-capable session state.
 *
 * Keeps memory requests on the application dispatch path while the session owns one of the
 * official active, memory-control, or memory-response states.
 *
 * @param[in] session Current Xbox GIP session.
 * @param[in] request Received Xbox GIP endpoint packet.
 * @return True when the packet belongs to the active memory path.
 */
static bool is_memory_packet(const UsbXboxGipSession *session,
                             const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    return request[0] == XBOX_GIP_MEMORY_PACKET &&
           (session->state == USB_XBOX_GIP_SESSION_ACTIVE ||
            session->state == USB_XBOX_GIP_SESSION_MEMORY_CONTROL ||
            session->state == USB_XBOX_GIP_SESSION_MEMORY_RESPONSE);
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
 * @brief Processes one Xbox GIP metadata acknowledgement.
 *
 * Leaves the transfer waiting for the same packet when an acknowledgement is rejected and requests
 * application redispatch while the session is active.
 *
 * @param[in,out] service Active GIP service.
 * @param[in] request Current received packet.
 * @param[in] request_received Whether the request buffer contains a packet received this cycle.
 * @param[in,out] dispatch_request Set when the received request must reach application handlers.
 */
static void
service_metadata_acknowledgement(UsbXboxGipService *service,
                                 const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE],
                                 bool request_received, bool *dispatch_request) {
    if (request_received && service->metadata_download.awaiting_acknowledgement &&
        !usb_xbox_gip_metadata_download_acknowledge(&service->metadata_download, request) &&
        service->session.state == USB_XBOX_GIP_SESSION_ACTIVE) {
        *dispatch_request = true;
    }
}

/**
 * @brief Services an active Xbox GIP metadata download.
 *
 * Completes metadata setup, checks acknowledgement progress, and emits the next metadata packet
 * when its acknowledgement boundary permits progress.
 *
 * @param[in,out] service Active GIP service.
 * @param[in] identity Identity data containing the metadata document.
 * @param[in] request Current received packet.
 * @param[in] request_received Whether the request buffer contains a packet received this cycle.
 * @param[in,out] dispatch_request Set when the received request must reach application handlers.
 * @param[out] response Destination for the next metadata packet.
 * @return Metadata response length, or zero while waiting for acknowledgement.
 */
static uint8_t service_metadata(UsbXboxGipService *service,
                                const UsbXboxGipServiceIdentity *identity,
                                const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE],
                                bool request_received, bool *dispatch_request,
                                uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    if (service->metadata_pending) {
        usb_xbox_gip_session_finish_metadata(&service->session);
        service->metadata_pending = false;
    }
    if (service->metadata_download.awaiting_acknowledgement) {
        service_metadata_acknowledgement(service, request, request_received, dispatch_request);
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

void usb_xbox_gip_service_init(UsbXboxGipService *service) {
    *service = (UsbXboxGipService){.next_sequence = 1};
    usb_xbox_gip_discovery_init(&service->discovery);
    usb_xbox_gip_session_init(&service->session);
}

UsbXboxGipServiceResult
usb_xbox_gip_service_poll(UsbXboxGipService *service, const UsbXboxGipServiceIdentity *identity,
                          const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE],
                          bool request_received, uint32_t now,
                          uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE]) {
    UsbXboxGipServiceResult result = {
        .dispatch_request = request_received && (is_force_feedback_application_packet(request) ||
                                                 is_memory_packet(&service->session, request)),
    };
    if (service->metadata_active) {
        result.session_actions = usb_xbox_gip_session_handle(&service->session, request);
        result.response_length =
            emit_session_response(service, request, result.session_actions, response);
        if ((result.session_actions & USB_XBOX_GIP_SESSION_ACTION_RESET_FORCE_FEEDBACK) != 0) {
            usb_xbox_gip_session_finish_force_feedback_reset(&service->session);
        }
        if (result.session_actions != USB_XBOX_GIP_SESSION_ACTION_NONE) {
            service_metadata_acknowledgement(service, request, request_received,
                                             &result.dispatch_request);
            return result;
        }
        result.response_length = service_metadata(service, identity, request, request_received,
                                                  &result.dispatch_request, response);
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
