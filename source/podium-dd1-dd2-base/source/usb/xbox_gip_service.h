#ifndef OPENTEC_BASE_USB_XBOX_GIP_SERVICE_H
#define OPENTEC_BASE_USB_XBOX_GIP_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/xbox_gip_discovery.h"
#include "usb/xbox_gip_metadata_download.h"
#include "usb/xbox_gip_session.h"

/** @brief Device identity data supplied to the Xbox GIP endpoint service. */
typedef struct {
    BoardVariant variant;    /**< Base hardware variant. */
    uint8_t wheel_mode;      /**< Attached-wheel protocol mode. */
    const uint8_t *digest;   /**< Eight-byte device digest. */
    const uint8_t *metadata; /**< Encoded Xbox GIP metadata document. */
} UsbXboxGipServiceIdentity;

/** @brief Mutable Xbox GIP discovery, metadata, session, and sequence state. */
typedef struct {
    UsbXboxGipDiscovery discovery;                /**< Discovery state. */
    UsbXboxGipMetadataDownload metadata_download; /**< Metadata transfer state. */
    UsbXboxGipSession session;                    /**< Session state. */
    uint8_t next_sequence;                        /**< Next response sequence value. */
    bool metadata_pending; /**< Whether metadata setup still needs to advance the session. */
    bool metadata_active;  /**< Whether metadata packets are currently being transferred. */
} UsbXboxGipService;

/** @brief Outputs produced by one Xbox GIP endpoint service cycle. */
typedef struct {
    UsbXboxGipSessionAction session_actions; /**< Session actions accepted in the cycle. */
    uint8_t response_length;                 /**< Number of valid bytes in the response buffer. */
    bool dispatch_request; /**< Whether the received request must reach application handlers. */
} UsbXboxGipServiceResult;

/**
 * @brief Initializes the Xbox GIP endpoint service.
 *
 * Starts discovery and session state with response sequence 1 and no active metadata transfer.
 *
 * @param[out] service GIP service state to initialize.
 */
void usb_xbox_gip_service_init(UsbXboxGipService *service);

/**
 * @brief Services one Xbox GIP endpoint cycle.
 *
 * Runs discovery, starts and advances metadata transfer, applies session commands, and emits at
 * most one response packet for the cycle.
 *
 * @param[in,out] service Active GIP service.
 * @param[in] identity Base, wheel, digest, and metadata identity inputs.
 * @param[in] request Current 64-byte request packet, or an all-zero packet when none was received.
 * @param[in] request_received Whether the request buffer contains a packet received this cycle.
 * @param[in] now Current monotonic time in milliseconds.
 * @param[out] response Destination for a response packet.
 * @return Session actions, response length, and application dispatch handling for the cycle.
 */
UsbXboxGipServiceResult
usb_xbox_gip_service_poll(UsbXboxGipService *service, const UsbXboxGipServiceIdentity *identity,
                          const uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE],
                          bool request_received, uint32_t now,
                          uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE]);

#endif
