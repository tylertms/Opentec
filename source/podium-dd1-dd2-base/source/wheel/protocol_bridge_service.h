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
    WHEEL_PROTOCOL_BRIDGE_WRITE_ERROR,   /**< A callback write failed and needs endpoint recovery. */
    WHEEL_PROTOCOL_BRIDGE_STARTUP_RECOVERY, /**< Startup endpoint recovery is in progress. */
} WheelProtocolBridgePhase;

/** @brief Report identifiers selected by attached-wheel startup negotiation. */
enum {
    WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD = 0x15,
    WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED = 0x16,
};

/** @brief Attached-wheel protocol callback state and shared transport. */
typedef struct {
    CommandTransport *transport;    /**< Shared command transport. */
    WheelProtocolBridgePhase phase; /**< Current callback transfer phase. */
    uint8_t report_id;              /**< Negotiated callback report identifier. */
    uint16_t wait_calls;            /**< Consecutive busy polls for the active callback write. */
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
 * Starts one callback write through the report identifier selected during startup negotiation and
 * retains the completion state until it is taken. Requests received before a write is queued
 * refresh the target; requests received during a transfer or recovery are coalesced without
 * changing the active transaction or acknowledgement latch.
 *
 * @param[in,out] service Callback service to start.
 * @param[in] report_id Nonzero negotiated callback report identifier.
 * @return True when the request was accepted or coalesced; otherwise false.
 */
bool wheel_protocol_bridge_service_request(WheelProtocolBridgeService *service, uint8_t report_id);

/**
 * @brief Advances an attached-wheel protocol bridge callback.
 *
 * Queues the callback write through the negotiated report identifier and polls its completion. A
 * rejected or stalled transfer enters endpoint recovery after the official 500-poll wait limit.
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
