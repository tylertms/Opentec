#ifndef OPENTEC_BASE_WHEEL_PROTOCOL_BRIDGE_SERVICE_H
#define OPENTEC_BASE_WHEEL_PROTOCOL_BRIDGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Attached-wheel protocol callback transfer phase. */
typedef enum {
    WHEEL_PROTOCOL_BRIDGE_IDLE,
    WHEEL_PROTOCOL_BRIDGE_WRITE_READY,
    WHEEL_PROTOCOL_BRIDGE_WRITE_PENDING,
} WheelProtocolBridgePhase;

/** @brief Attached-wheel protocol callback state and shared transport. */
typedef struct {
    CommandTransport *transport;
    WheelProtocolBridgePhase phase;
    uint8_t endpoint_index;
    bool acknowledged;
} WheelProtocolBridgeService;

void wheel_protocol_bridge_service_init(WheelProtocolBridgeService *service,
                                        CommandTransport *transport);
bool wheel_protocol_bridge_service_request(WheelProtocolBridgeService *service);
void wheel_protocol_bridge_service_run(WheelProtocolBridgeService *service);
bool wheel_protocol_bridge_service_take_acknowledgement(WheelProtocolBridgeService *service);

#endif
