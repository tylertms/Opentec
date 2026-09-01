#ifndef OPENTEC_BASE_WHEEL_PROTOCOL_BRIDGE_SERVICE_H
#define OPENTEC_BASE_WHEEL_PROTOCOL_BRIDGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Attached-wheel protocol callback transfer phase. */
typedef enum {
    WHEEL_PROTOCOL_BRIDGE_IDLE,          /**< No callback request is active. */
    WHEEL_PROTOCOL_BRIDGE_WRITE_READY,   /**< The callback write may be queued. */
    WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING, /**< A callback write is awaiting completion. */
} WheelProtocolBridgePhase;

/** @brief Attached-wheel protocol callback state and shared transport. */
typedef struct {
    CommandTransport *transport;    /**< Shared command transport. */
    WheelProtocolBridgePhase phase; /**< Current callback transfer phase. */
    uint8_t endpoint_index;         /**< Endpoint currently being attempted. */
    bool acknowledged;              /**< One-shot successful-write latch. */
} WheelProtocolBridgeService;

/**
 * @brief Initializes the attached-wheel protocol callback service.
 *
 * Attaches the shared command transport and returns the callback state to idle.
 *
 * @param[out] service Callback service state to initialize.
 * @param[in] transport Shared command transport to attach.
 */
void wheel_protocol_bridge_service_init(WheelProtocolBridgeService *service,
                                        CommandTransport *transport);

/**
 * @brief Requests an attached-wheel protocol bridge callback.
 *
 * Starts an endpoint write attempt when the service is idle and retains the completion state until
 * it is taken.
 *
 * @param[in,out] service Callback service to start.
 * @return True when a new callback request started; otherwise false.
 */
bool wheel_protocol_bridge_service_request(WheelProtocolBridgeService *service);

/**
 * @brief Advances an attached-wheel protocol bridge callback.
 *
 * Queues the callback write, polls its completion, and retries the alternate endpoint after a
 * rejected transfer.
 *
 * @param[in,out] service Active callback service to advance.
 */
void wheel_protocol_bridge_service_run(WheelProtocolBridgeService *service);

/**
 * @brief Takes a completed attached-wheel protocol callback acknowledgement.
 *
 * Clears the one-shot completion latch after returning it to the caller.
 *
 * @param[in,out] service Callback service holding the completion latch.
 * @return True once after a successful callback write; otherwise false.
 */
bool wheel_protocol_bridge_service_take_acknowledgement(WheelProtocolBridgeService *service);

#endif
