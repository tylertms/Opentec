#ifndef OPENTEC_BASE_WHEEL_UPDATER_DIRECT_SERVICE_H
#define OPENTEC_BASE_WHEEL_UPDATER_DIRECT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/updater_bridge.h"

/** @brief Raw-UART adapter for the wheel updater protocol. */
typedef struct {
    WheelUpdaterBridge bridge; /**< Transport-independent updater protocol state. */
    uint8_t read_buffer[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE]; /**< Direct-UART read buffer. */
    uint8_t pending_length; /**< Byte count stored for a pending direct-UART operation. */
    WheelUpdaterOperationKind
        pending_operation;  /**< Operation kind stored for a pending direct-UART operation. */
    bool operation_pending; /**< True while a direct-UART operation is active. */
} WheelUpdaterDirectService;

/**
 * @brief Initializes the direct-UART updater service.
 *
 * Clears the updater bridge, read buffer, and pending-operation state.
 *
 * @param[out] service Direct updater service to initialize; null is ignored.
 */
void wheel_updater_direct_service_init(WheelUpdaterDirectService *service);

/**
 * @brief Starts a direct-UART updater exchange.
 *
 * Delegates marker, length, and bridge-state validation to the transport-independent updater
 * protocol.
 *
 * @param[in,out] service Idle direct updater service to start.
 * @param[in] request Marker-prefixed updater request bytes.
 * @param[in] length Request length in bytes.
 * @return True when the request is valid and accepted; otherwise false.
 */
bool wheel_updater_direct_service_start(WheelUpdaterDirectService *service, const uint8_t *request,
                                        uint8_t length);

/**
 * @brief Starts a route-discovery probe on the raw attached-wheel link.
 *
 * Applies the same ownership checks as #wheel_updater_direct_service_start while preserving the
 * probe-only terminal response rules.
 *
 * @param[in,out] service Idle direct updater service to start.
 * @param[in] request Marker-prefixed route-probe request bytes.
 * @param[in] length Probe request length in bytes.
 * @return True when the probe was accepted; otherwise false.
 */
bool wheel_updater_direct_service_start_probe(WheelUpdaterDirectService *service,
                                              const uint8_t *request, uint8_t length);

/**
 * @brief Advances direct-UART updater work.
 *
 * Polls the pending UART operation, advances the updater bridge, and starts its next raw read or
 * write operation.
 *
 * @param[in,out] service Active direct updater service to advance; null is ignored.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_updater_direct_service_run(WheelUpdaterDirectService *service, uint32_t now_ms);

/**
 * @brief Takes a complete direct-UART updater response.
 *
 * Returns the bridge's retained response and clears its response-ready phase.
 *
 * @param[in,out] service Direct updater service holding a response.
 * @param[out] response Receives a pointer to retained response bytes.
 * @param[out] length Receives the response length.
 * @return True when a complete response was available; otherwise false.
 */
bool wheel_updater_direct_service_take_response(WheelUpdaterDirectService *service,
                                                const uint8_t **response, uint8_t *length);

/**
 * @brief Reports whether direct-UART updater work is active.
 *
 * Includes queued and pending UART operations plus an untaken complete response.
 *
 * @param[in] service Direct updater service to inspect.
 * @return True while service is non-null and owns an updater exchange; otherwise false.
 */
bool wheel_updater_direct_service_active(const WheelUpdaterDirectService *service);

#endif
