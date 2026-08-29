#ifndef OPENTEC_BASE_WHEEL_UPDATER_AUX_SERVICE_H
#define OPENTEC_BASE_WHEEL_UPDATER_AUX_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/updater_bridge.h"

typedef struct {
    WheelUpdaterBridge bridge;
    uint8_t read_buffer[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE];
    WheelUpdaterOperationKind pending_operation;
    uint8_t pending_length;
    bool transfer_active;
    bool handshake_requested;
    bool handshake_complete;
} WheelUpdaterAuxService;

void wheel_updater_aux_service_init(WheelUpdaterAuxService *service);
void wheel_updater_aux_service_request_handshake(WheelUpdaterAuxService *service);
bool wheel_updater_aux_service_handshake_complete(const WheelUpdaterAuxService *service);
bool wheel_updater_aux_service_start(WheelUpdaterAuxService *service, const uint8_t *request,
                                     uint8_t length);
void wheel_updater_aux_service_run(WheelUpdaterAuxService *service, uint32_t now_ms);
bool wheel_updater_aux_service_take_response(WheelUpdaterAuxService *service,
                                             const uint8_t **response, uint8_t *length);
bool wheel_updater_aux_service_active(const WheelUpdaterAuxService *service);

#endif
