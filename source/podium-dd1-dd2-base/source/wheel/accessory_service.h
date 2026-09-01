#ifndef OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H
#define OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/accessory.h"

/**
 * @brief Attached wheel accessory polling and identity state.
 *
 * The service advances status, version, and extended accessory-type reads one transport request
 * at a time while retaining the last accepted identity.
 */
typedef struct {
    WheelAccessory accessory;    /**< Last accepted accessory identity and protocol state. */
    uint8_t version_bytes[4];    /**< Four bytes retained for the little-endian version value. */
    uint8_t status_byte;         /**< Signed status byte retained from the status read. */
    uint8_t accessory_type_byte; /**< Type byte retained from an extended accessory read. */
    bool version_stage;          /**< True when the next request reads the accessory version. */
    bool accessory_type_stage;   /**< True when the next request reads the extended type byte. */
    bool request_pending; /**< True while the command transport owns an active service request. */
} WheelAccessoryService;

/**
 * @brief Initializes attached wheel accessory polling.
 *
 * Clears pending response storage, starts at the status stage, and initializes the logical
 * accessory identity as disconnected. A null destination is ignored.
 *
 * @param[out] service Accessory service to initialize.
 */
void wheel_accessory_service_init(WheelAccessoryService *service);

/**
 * @brief Advances attached wheel accessory identity polling.
 *
 * Completes the active request or queues the next status, version, or accessory-type read using
 * the shared command transport. A null service or transport is ignored.
 *
 * @param[in,out] service Accessory service to advance.
 * @param[in,out] transport Shared command transport used by the service.
 */
void wheel_accessory_service_run(WheelAccessoryService *service, CommandTransport *transport);

/**
 * @brief Returns the latest attached wheel accessory identity.
 *
 * Returns a pointer to the identity retained inside service; the pointer remains valid while
 * service is unchanged.
 *
 * @param[in] service Accessory service to inspect.
 * @return Pointer to the retained identity, or null when service is null.
 */
const WheelAccessory *wheel_accessory_service_identity(const WheelAccessoryService *service);

#endif
