#ifndef OPENTEC_BASE_WHEEL_UPDATER_AUX_SERVICE_H
#define OPENTEC_BASE_WHEEL_UPDATER_AUX_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/updater_bridge.h"

/** @brief Auxiliary-bus adapter for the transport-independent updater bridge. */
typedef struct {
    WheelUpdaterBridge bridge; /**< Transport-independent updater protocol state. */
    uint8_t read_buffer[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE]; /**< Auxiliary read buffer. */
    WheelUpdaterOperationKind pending_operation; /**< Operation currently active on the bus. */
    uint8_t pending_length;   /**< Number of bytes requested by the pending read. */
    bool transfer_active;     /**< True while an auxiliary-bus transaction is active. */
    bool handshake_requested; /**< True while the shutdown handshake awaits a bus attempt. */
    bool handshake_complete;  /**< True after the shutdown handshake succeeds. */
} WheelUpdaterAuxService;

/**
 * @brief Initializes the auxiliary updater service.
 *
 * Clears handshake, bus-transfer, and updater bridge state.
 *
 * @param[out] service Auxiliary updater service to initialize; null is ignored.
 */
void wheel_updater_aux_service_init(WheelUpdaterAuxService *service);

/**
 * @brief Prepares auxiliary updater startup recovery.
 *
 * Marks the separate shutdown prerequisite complete when the service has no active transfer or
 * updater exchange.
 *
 * @param[in,out] service Auxiliary updater service to prepare; null is ignored.
 */
void wheel_updater_aux_service_prepare_startup_recovery(WheelUpdaterAuxService *service);

/**
 * @brief Requests the auxiliary shutdown handshake.
 *
 * Schedules the handshake write unless the handshake has already completed.
 *
 * @param[in,out] service Auxiliary updater service receiving the request; null is ignored.
 */
void wheel_updater_aux_service_request_handshake(WheelUpdaterAuxService *service);

/**
 * @brief Reports whether the auxiliary shutdown handshake completed.
 *
 * Reads the retained completion latch without consuming it.
 *
 * @param[in] service Auxiliary updater service to inspect.
 * @return True after the handshake succeeds; otherwise false.
 */
bool wheel_updater_aux_service_handshake_complete(const WheelUpdaterAuxService *service);

/**
 * @brief Starts an auxiliary updater exchange.
 *
 * Requires the shutdown handshake to be complete and delegates request validation to the updater
 * bridge.
 *
 * @param[in,out] service Idle auxiliary updater service to start.
 * @param[in] request Marker-prefixed updater request bytes.
 * @param[in] length Request length in bytes.
 * @return True when the request is valid and accepted; otherwise false.
 */
bool wheel_updater_aux_service_start(WheelUpdaterAuxService *service, const uint8_t *request,
                                     uint8_t length);

/**
 * @brief Advances auxiliary handshake or updater work.
 *
 * Polls the auxiliary bus, completes the shutdown handshake when requested, and advances the
 * transport-independent updater bridge.
 *
 * @param[in,out] service Auxiliary updater service to advance; null is ignored.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_updater_aux_service_run(WheelUpdaterAuxService *service, uint32_t now_ms);

/**
 * @brief Takes a complete auxiliary updater response.
 *
 * Returns the bridge's retained response and clears its response-ready phase.
 *
 * @param[in,out] service Auxiliary updater service holding a response.
 * @param[out] response Receives a pointer to retained response bytes.
 * @param[out] length Receives the response length.
 * @return True when a complete response was available; otherwise false.
 */
bool wheel_updater_aux_service_take_response(WheelUpdaterAuxService *service,
                                             const uint8_t **response, uint8_t *length);

/**
 * @brief Reports whether auxiliary updater work is active.
 *
 * Includes a pending handshake, active bus transfer, or non-idle updater bridge phase.
 *
 * @param[in] service Auxiliary updater service to inspect.
 * @return True while service is non-null and owns pending work; otherwise false.
 */
bool wheel_updater_aux_service_active(const WheelUpdaterAuxService *service);

#endif
