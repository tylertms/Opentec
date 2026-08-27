#ifndef OPENTEC_BASE_WHEEL_SERVICE_H
#define OPENTEC_BASE_WHEEL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/protocol.h"
#include "wheel/transport_service.h"

enum { WHEEL_BUTTON_BANK_COUNT = 3 };

typedef enum {
    WHEEL_SERVICE_REQUEST_NONE,
    WHEEL_SERVICE_REQUEST_PROTOCOL,
    WHEEL_SERVICE_REQUEST_BUTTONS,
} WheelServiceRequest;

typedef struct {
    WheelTransportService transport;
    WheelProtocol protocol;
    uint8_t request[WHEEL_TRANSPORT_PAYLOAD_SIZE];
    uint8_t button_banks[WHEEL_BUTTON_BANK_COUNT];
    uint8_t scan_phase;
    WheelServiceRequest request_kind;
} WheelService;

void wheel_service_init(WheelService *service);
void wheel_service_run(WheelService *service, uint32_t now_ms);
const uint8_t *wheel_service_buttons(const WheelService *service);
uint8_t wheel_service_mode(const WheelService *service);
WheelProtocolPhase wheel_service_protocol_phase(const WheelService *service);

#endif
