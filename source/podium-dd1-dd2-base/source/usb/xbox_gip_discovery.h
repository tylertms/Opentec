#ifndef OPENTEC_BASE_USB_XBOX_GIP_DISCOVERY_H
#define OPENTEC_BASE_USB_XBOX_GIP_DISCOVERY_H

#include <stdint.h>

/** @brief Xbox GIP discovery timing constants. */
enum {
    USB_XBOX_GIP_DISCOVERY_TIMEOUT_MS = 500 /**< Milliseconds before the digest is sent again. */
};

/** @brief Phase of the Xbox GIP discovery exchange. */
typedef enum {
    USB_XBOX_GIP_DISCOVERY_SEND_DIGEST,      /**< Send the discovery digest. */
    USB_XBOX_GIP_DISCOVERY_WAIT_FOR_REQUEST, /**< Wait for metadata or session selection. */
} UsbXboxGipDiscoveryPhase;

/** @brief Action produced by one Xbox GIP discovery poll. */
typedef enum {
    USB_XBOX_GIP_DISCOVERY_IDLE,            /**< No discovery response is required. */
    USB_XBOX_GIP_DISCOVERY_DIGEST,          /**< Send the discovery digest. */
    USB_XBOX_GIP_DISCOVERY_METADATA,        /**< Start metadata download. */
    USB_XBOX_GIP_DISCOVERY_SESSION_COMMAND, /**< Handle a session command. */
} UsbXboxGipDiscoveryAction;

/** @brief Xbox GIP discovery phase and request deadline. */
typedef struct {
    uint32_t deadline;              /**< Monotonic time at which the discovery wait expires. */
    UsbXboxGipDiscoveryPhase phase; /**< Current discovery phase. */
} UsbXboxGipDiscovery;

/**
 * @brief Initializes Xbox GIP discovery.
 *
 * Selects the digest phase so the next service cycle announces the device before accepting a
 * metadata or session request.
 *
 * @param[out] discovery Discovery state to initialize.
 */
void usb_xbox_gip_discovery_init(UsbXboxGipDiscovery *discovery);

/**
 * @brief Advances Xbox GIP discovery.
 *
 * Requests a digest, waits 500 milliseconds for metadata request 4 or session request 5, and
 * returns to the digest phase only after the deadline has passed.
 *
 * @param[in,out] discovery Active discovery state.
 * @param[in] request_id First byte of the received request packet.
 * @param[in] now Current monotonic time in milliseconds.
 * @return Discovery action for the current service cycle.
 */
UsbXboxGipDiscoveryAction usb_xbox_gip_discovery_poll(UsbXboxGipDiscovery *discovery,
                                                      uint8_t request_id, uint32_t now);

#endif
