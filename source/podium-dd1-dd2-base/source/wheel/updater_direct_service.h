#ifndef OPENTEC_BASE_WHEEL_UPDATER_DIRECT_SERVICE_H
#define OPENTEC_BASE_WHEEL_UPDATER_DIRECT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/updater_bridge.h"

/** @brief Raw-UART adapter for the wheel updater protocol. */
typedef struct {
    WheelUpdaterBridge bridge;
    uint8_t read_buffer[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE];
    uint8_t pending_length;
    WheelUpdaterOperationKind pending_operation;
    bool operation_pending;
} WheelUpdaterDirectService;

void wheel_updater_direct_service_init(WheelUpdaterDirectService *service);
bool wheel_updater_direct_service_start(WheelUpdaterDirectService *service, const uint8_t *request,
                                        uint8_t length);
void wheel_updater_direct_service_run(WheelUpdaterDirectService *service, uint32_t now_ms);
bool wheel_updater_direct_service_take_response(WheelUpdaterDirectService *service,
                                                const uint8_t **response, uint8_t *length);
bool wheel_updater_direct_service_active(const WheelUpdaterDirectService *service);

#endif
